# Phase 2A — WiFi + NTP + REST API Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get the tipboard online with WiFi, display real time via NTP, and expose a REST API for remote status control.

**Architecture:** New `network` component handles WiFi STA connection (with AP fallback for provisioning) and NTP sync. New `webserver` component runs `esp_http_server` with JSON REST endpoints and WebSocket for live state push. Credentials stored in NVS. Time display updated in the existing UI top bar. All network activity runs on Core 1; LVGL stays on Core 0. State changes from API are marshalled to LVGL context via the existing `state_change_cb` + `lv_timer` pattern.

**Tech Stack:** ESP-IDF v5.5.3 (`esp_wifi`, `esp_netif`, `esp_netif_sntp`, `esp_http_server` with WebSocket, `nvs_flash`, `cJSON`)

**Current State (Phase 1):** Three-zone LVGL UI with 7 modes, touch cycling, Pomodoro, LittleFS persistence. Top bar shows "--:--" placeholder. No WiFi.

---

## File Structure

```
components/
  network/
    include/
      network.h            — WiFi init, connect, AP mode, status query
      ntp.h                — NTP sync init, time query
    network.c              — WiFi STA/AP logic, event handlers, reconnect
    ntp.c                  — SNTP init, timezone, time formatting
    CMakeLists.txt
  webserver/
    include/
      webserver.h          — HTTP server init, start/stop
    webserver.c            — REST API endpoints, WebSocket push
    CMakeLists.txt
  state/                   (modify)
    state.c                — Add thread-safe state_set_mode_from_api() with LVGL marshalling
  ui/                      (modify)
    ui_screen.c            — Update top bar with real time, show WiFi status
main/
  main.c                   — Add WiFi+NTP+webserver init, NVS init
```

**Component dependency graph:**
```
main → network, webserver, state, ui, board, lvgl
network → (esp_wifi, esp_netif, esp_netif_sntp, nvs_flash — all ESP-IDF built-in)
webserver → esp_http_server, state, json
state → (unchanged deps)
ui → lvgl, state (unchanged)
```

---

## Chunk 1: WiFi Connection

### Task 1: Create Network Component — WiFi STA

**Files:**
- Create: `components/network/CMakeLists.txt`
- Create: `components/network/include/network.h`
- Create: `components/network/network.c`

- [ ] **Step 1: Create `components/network/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "network.c" "ntp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_netif_sntp log
)
```

- [ ] **Step 2: Create `components/network/include/network.h`**

```c
#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* WiFi connection states */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,       /* Provisioning AP active */
} wifi_state_t;

/* Initialize WiFi subsystem (NVS, netif, event loop).
 * Call once at boot before network_wifi_connect(). */
esp_err_t network_init(void);

/* Connect to WiFi using credentials from NVS.
 * If no credentials stored, starts AP mode for provisioning.
 * Non-blocking — connection happens via event handlers. */
esp_err_t network_wifi_connect(void);

/* Store WiFi credentials to NVS (used by provisioning AP) */
esp_err_t network_set_credentials(const char *ssid, const char *password);

/* Get current WiFi state */
wifi_state_t network_get_state(void);

/* Get IP address as string (empty if not connected) */
const char *network_get_ip(void);

/* Get SSID we're connected to (empty if not connected) */
const char *network_get_ssid(void);
```

- [ ] **Step 3: Create `components/network/network.c`**

```c
#include "network.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "network";

#define NVS_NAMESPACE     "tipboard"
#define NVS_KEY_SSID      "wifi_ssid"
#define NVS_KEY_PASS      "wifi_pass"
#define AP_SSID            "Tipboard-Setup"
#define MAX_RETRY          5

static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static char s_ip_str[16] = "";
static char s_ssid[33] = "";
static int s_retry_count = 0;
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            s_state = WIFI_STATE_CONNECTING;
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            s_state = WIFI_STATE_DISCONNECTED;
            s_ip_str[0] = '\0';
            if (s_retry_count < MAX_RETRY) {
                s_retry_count++;
                ESP_LOGI(TAG, "Retry WiFi connection (%d/%d)", s_retry_count, MAX_RETRY);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "WiFi connection failed after %d retries", MAX_RETRY);
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected! IP: %s", s_ip_str);
        s_state = WIFI_STATE_CONNECTED;
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t network_init(void)
{
    /* Initialize NVS — required for WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize TCP/IP and event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_wifi_event_group = xEventGroupCreate();

    return ESP_OK;
}

static esp_err_t load_credentials(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_get_str(nvs, NVS_KEY_SSID, ssid, &ssid_len);
    if (err == ESP_OK) {
        nvs_get_str(nvs, NVS_KEY_PASS, pass, &pass_len);
    }
    nvs_close(nvs);
    return err;
}

static void start_ap_mode(void)
{
    ESP_LOGI(TAG, "Starting AP mode: %s", AP_SSID);
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_state = WIFI_STATE_AP_MODE;
    /* IP for AP mode is always 192.168.4.1 */
    strncpy(s_ip_str, "192.168.4.1", sizeof(s_ip_str));
    ESP_LOGI(TAG, "AP mode active at %s", s_ip_str);
}

esp_err_t network_wifi_connect(void)
{
    char ssid[33] = {0};
    char pass[65] = {0};

    /* Try to load saved credentials */
    esp_err_t err = load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    if (err != ESP_OK || ssid[0] == '\0') {
        ESP_LOGW(TAG, "No WiFi credentials found, starting AP mode");
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                        &wifi_event_handler, NULL, NULL));
        start_ap_mode();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t network_set_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    nvs_set_str(nvs, NVS_KEY_SSID, ssid);
    nvs_set_str(nvs, NVS_KEY_PASS, password ? password : "");
    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "WiFi credentials saved for: %s", ssid);
    return ESP_OK;
}

wifi_state_t network_get_state(void) { return s_state; }
const char *network_get_ip(void) { return s_ip_str; }
const char *network_get_ssid(void) { return s_ssid; }
```

- [ ] **Step 4: Stub `components/network/ntp.c`**

Create minimal stub so component compiles:

```c
#include "ntp.h"

/* Stub — implemented in Task 2 */
esp_err_t ntp_init(void) { return ESP_OK; }
bool ntp_is_synced(void) { return false; }
void ntp_get_time_str(char *buf, size_t len) { buf[0] = '\0'; }
```

- [ ] **Step 5: Create stub `components/network/include/ntp.h`**

```c
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/* Start NTP sync. Call after WiFi is connected. */
esp_err_t ntp_init(void);

/* Check if time has been synced at least once */
bool ntp_is_synced(void);

/* Get formatted time string (e.g., "2:45 PM") into buffer */
void ntp_get_time_str(char *buf, size_t len);

/* Get formatted date string (e.g., "Sat, Mar 15") into buffer */
void ntp_get_date_str(char *buf, size_t len);
```

- [ ] **Step 6: Update `main/main.c` — add network init**

After board/LVGL init, before UI init:

```c
#include "network.h"
#include "ntp.h"

/* In app_main(), after LVGL setup: */
ESP_ERROR_CHECK(network_init());
network_wifi_connect();
```

Add `network` to `main/CMakeLists.txt` REQUIRES:

```cmake
REQUIRES board state ui network lvgl esp_timer esp_psram
```

- [ ] **Step 7: Add WiFi credentials for first boot**

For development, hardcode initial credentials via a one-time NVS write, or store them before first connect. The simplest approach: call `network_set_credentials()` before `network_wifi_connect()` if no credentials exist yet. This can be removed once the provisioning AP + web dashboard are working in Phase 2B.

Alternatively, add a `wifi_credentials.h` (gitignored) with SSID/password defines.

- [ ] **Step 8: Build and verify WiFi connects**

Clean rebuild (new component):
```bash
rm -f sdkconfig && rm -rf build
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```

Check serial output for: `Connected! IP: 192.168.x.x`

- [ ] **Step 9: Commit**

```bash
git add components/network/ main/
git commit -m "feat(network): WiFi STA connection with NVS credential storage and AP fallback"
```

---

## Chunk 2: NTP Time Sync + Top Bar Clock

### Task 2: NTP Sync and Time Display

**Files:**
- Modify: `components/network/ntp.c` — full SNTP implementation
- Modify: `components/network/include/ntp.h` — add date_str
- Modify: `components/ui/ui_screen.c` — update top bar with real time
- Modify: `main/main.c` — start NTP after WiFi connects

- [ ] **Step 1: Implement `components/network/ntp.c`**

```c
#include "ntp.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

static const char *TAG = "ntp";
static bool s_synced = false;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
    s_synced = true;
}

esp_err_t ntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Set timezone — Eastern Time (configurable later via API) */
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();

    return ESP_OK;
}

bool ntp_is_synced(void)
{
    return s_synced;
}

void ntp_get_time_str(char *buf, size_t len)
{
    if (!s_synced || len < 8) {
        strncpy(buf, "--:--", len);
        return;
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    /* 12-hour format: "2:45 PM" */
    int hour = timeinfo.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char *ampm = timeinfo.tm_hour >= 12 ? "PM" : "AM";
    snprintf(buf, len, "%d:%02d %s", hour, timeinfo.tm_min, ampm);
}

void ntp_get_date_str(char *buf, size_t len)
{
    if (!s_synced || len < 12) {
        strncpy(buf, "", len);
        return;
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    /* "Sat, Mar 15" */
    strftime(buf, len, "%a, %b %d", &timeinfo);
}
```

- [ ] **Step 2: Update top bar clock in `ui_screen.c`**

The existing 1-second `lv_timer` (in `main.c`) calls `ui_update_timer()`. Add a time display update path. The simplest approach: add a function to `ui.h`:

```c
/* Update top bar time display. Called every ~30s or on NTP sync. */
void ui_update_time(const char *time_str, const char *date_str);
```

Implement in `ui_screen.c`:

```c
void ui_update_time(const char *time_str, const char *date_str)
{
    if (!s_time_label) return;

    if (date_str && date_str[0]) {
        lv_label_set_text_fmt(s_time_label, "%s . %s", time_str, date_str);
    } else {
        lv_label_set_text(s_time_label, time_str);
    }
}
```

- [ ] **Step 3: Add time update to the 1-second LVGL timer in `main.c`**

Update the `one_second_lv_timer_cb` to refresh the clock every 30 seconds (not every second — no need):

```c
static int s_time_update_counter = 0;

static void one_second_lv_timer_cb(lv_timer_t *timer)
{
    state_tick();

    const status_state_t *state = state_get();
    int32_t seconds = state_timer_get_seconds();
    ui_update_timer(seconds, state->timer_type);

    /* Update clock every 30 seconds */
    if (++s_time_update_counter >= 30) {
        s_time_update_counter = 0;
        char time_str[16], date_str[16];
        ntp_get_time_str(time_str, sizeof(time_str));
        ntp_get_date_str(date_str, sizeof(date_str));
        ui_update_time(time_str, date_str);
    }
}
```

- [ ] **Step 4: Start NTP after WiFi connects in `main.c`**

Add NTP init after WiFi connect call. NTP will automatically sync once WiFi gets an IP:

```c
network_wifi_connect();
ntp_init();  /* Starts SNTP — will sync when WiFi connects */
```

- [ ] **Step 5: Build, flash, verify time appears in top bar**

Expected serial output: `Time synchronized`
Expected display: Top bar shows real time like "2:45 PM . Sat, Mar 15"

- [ ] **Step 6: Commit**

```bash
git add components/network/ components/ui/ main/
git commit -m "feat(ntp): NTP time sync with live clock in top bar"
```

---

## Chunk 3: REST API

### Task 3: HTTP Server with REST Endpoints

**Files:**
- Create: `components/webserver/CMakeLists.txt`
- Create: `components/webserver/include/webserver.h`
- Create: `components/webserver/webserver.c`
- Modify: `main/main.c` — start webserver after WiFi connects

The REST API from the design spec:

| Endpoint | Method | Purpose |
|---|---|---|
| `GET /api/status` | GET | Current status + full state |
| `PUT /api/status` | PUT | Set mode, subtitle, timer, auto-expire |
| `POST /api/timer/start` | POST | Start timer (elapsed or pomodoro) |
| `POST /api/timer/stop` | POST | Stop/reset timer |
| `GET /api/modes` | GET | List available modes + color schemes |
| `GET /api/version` | GET | Firmware version |
| `WS /ws` | WS | Live state updates pushed to clients |

- [ ] **Step 1: Create `components/webserver/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json state network log
)
```

- [ ] **Step 2: Create `components/webserver/include/webserver.h`**

```c
#pragma once

#include "esp_err.h"

/* Start the HTTP server (call after WiFi is connected) */
esp_err_t webserver_start(void);

/* Stop the HTTP server */
esp_err_t webserver_stop(void);

/* Push state update to all connected WebSocket clients.
 * Called from state change callback. */
void webserver_notify_clients(void);
```

- [ ] **Step 3: Create `components/webserver/webserver.c`**

```c
#include "webserver.h"
#include "state.h"
#include "network.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "webserver";
static httpd_handle_t s_server = NULL;

/* ── JSON helpers ── */

static cJSON *state_to_json(const status_state_t *state)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", state_mode_label(state->mode));
    cJSON_AddNumberToObject(root, "mode_id", state->mode);
    cJSON_AddStringToObject(root, "subtitle", state->subtitle);
    cJSON_AddNumberToObject(root, "timer_type", state->timer_type);
    cJSON_AddNumberToObject(root, "timer_seconds", state_timer_get_seconds());
    cJSON_AddNumberToObject(root, "timer_duration", state->timer_duration_sec);
    cJSON_AddNumberToObject(root, "pomo_phase", state->pomo_phase);
    cJSON_AddNumberToObject(root, "priority", state->priority);
    cJSON_AddStringToObject(root, "source",
        state->source == SOURCE_MANUAL ? "manual" :
        state->source == SOURCE_API ? "api" :
        state->source == SOURCE_MQTT ? "mqtt" : "keypad");
    return root;
}

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *str = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, str);
    cJSON_free(str);
    cJSON_Delete(json);
    return ESP_OK;
}

/* ── GET /api/status ── */

static esp_err_t api_get_status(httpd_req_t *req)
{
    const status_state_t *state = state_get();
    cJSON *json = state_to_json(state);
    return send_json_response(req, json);
}

/* ── PUT /api/status ── */

static esp_err_t api_put_status(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (mode && cJSON_IsNumber(mode)) {
        int mode_id = mode->valueint;
        if (mode_id >= 0 && mode_id < MODE_COUNT) {
            state_set_mode((status_mode_t)mode_id, SOURCE_API);
        }
    }

    cJSON *subtitle = cJSON_GetObjectItem(root, "subtitle");
    if (subtitle && cJSON_IsString(subtitle)) {
        state_set_subtitle(subtitle->valuestring);
    }

    cJSON_Delete(root);

    /* Return updated state */
    const status_state_t *state = state_get();
    cJSON *resp = state_to_json(state);
    return send_json_response(req, resp);
}

/* ── POST /api/timer/start ── */

static esp_err_t api_timer_start(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type && cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "pomodoro") == 0) {
            const status_state_t *s = state_get();
            state_pomodoro_start(s->pomo_work_sec, s->pomo_break_sec);
        } else if (strcmp(type->valuestring, "elapsed") == 0) {
            state_timer_start_elapsed();
        }
    }

    cJSON_Delete(root);

    const status_state_t *state = state_get();
    cJSON *resp = state_to_json(state);
    return send_json_response(req, resp);
}

/* ── POST /api/timer/stop ── */

static esp_err_t api_timer_stop(httpd_req_t *req)
{
    const status_state_t *state = state_get();
    if (state->mode == MODE_POMODORO) {
        state_pomodoro_cancel();
    } else {
        state_timer_stop();
    }

    state = state_get();
    cJSON *resp = state_to_json(state);
    return send_json_response(req, resp);
}

/* ── GET /api/modes ── */

static esp_err_t api_get_modes(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < MODE_COUNT; i++) {
        cJSON *mode = cJSON_CreateObject();
        cJSON_AddNumberToObject(mode, "id", i);
        cJSON_AddStringToObject(mode, "label", state_mode_label(i));
        cJSON_AddItemToArray(root, mode);
    }

    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, str);
    cJSON_free(str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── GET /api/version ── */

static esp_err_t api_get_version(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", "1.0.0-phase2a");
    cJSON_AddStringToObject(root, "api", "1.0");
    cJSON_AddStringToObject(root, "board", "JC1060P470C");
    return send_json_response(req, root);
}

/* ── WebSocket /ws ── */

#define MAX_WS_CLIENTS 4
static int s_ws_fds[MAX_WS_CLIENTS];
static int s_ws_count = 0;

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* New WebSocket connection */
        int fd = httpd_req_to_sockfd(req);
        if (s_ws_count < MAX_WS_CLIENTS) {
            s_ws_fds[s_ws_count++] = fd;
            ESP_LOGI(TAG, "WebSocket client connected (fd=%d, total=%d)", fd, s_ws_count);
        }

        /* Send current state immediately */
        const status_state_t *state = state_get();
        cJSON *json = state_to_json(state);
        char *str = cJSON_PrintUnformatted(json);

        httpd_ws_frame_t ws_pkt = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)str,
            .len = strlen(str),
        };
        httpd_ws_send_frame(req, &ws_pkt);

        cJSON_free(str);
        cJSON_Delete(json);
        return ESP_OK;
    }

    /* Handle incoming WebSocket frames (we mostly ignore them) */
    httpd_ws_frame_t ws_pkt = {0};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    /* Allocate buffer for payload if needed */
    if (ws_pkt.len > 0) {
        ws_pkt.payload = malloc(ws_pkt.len + 1);
        if (!ws_pkt.payload) return ESP_ERR_NO_MEM;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        free(ws_pkt.payload);
    }

    return ESP_OK;
}

void webserver_notify_clients(void)
{
    if (!s_server || s_ws_count == 0) return;

    const status_state_t *state = state_get();
    cJSON *json = state_to_json(state);
    char *str = cJSON_PrintUnformatted(json);

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)str,
        .len = strlen(str),
    };

    /* Send to all connected WS clients, remove dead ones */
    int new_count = 0;
    for (int i = 0; i < s_ws_count; i++) {
        esp_err_t err = httpd_ws_send_frame_async(s_server, s_ws_fds[i], &ws_pkt);
        if (err == ESP_OK) {
            s_ws_fds[new_count++] = s_ws_fds[i];
        } else {
            ESP_LOGW(TAG, "WS client fd=%d disconnected", s_ws_fds[i]);
        }
    }
    s_ws_count = new_count;

    cJSON_free(str);
    cJSON_Delete(json);
}

/* ── CORS preflight ── */

static esp_err_t cors_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ── Server start/stop ── */

esp_err_t webserver_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    /* Register REST endpoints */
    const httpd_uri_t uris[] = {
        { .uri = "/api/status",      .method = HTTP_GET,     .handler = api_get_status },
        { .uri = "/api/status",      .method = HTTP_PUT,     .handler = api_put_status },
        { .uri = "/api/timer/start", .method = HTTP_POST,    .handler = api_timer_start },
        { .uri = "/api/timer/stop",  .method = HTTP_POST,    .handler = api_timer_stop },
        { .uri = "/api/modes",       .method = HTTP_GET,     .handler = api_get_modes },
        { .uri = "/api/version",     .method = HTTP_GET,     .handler = api_get_version },
        { .uri = "/api/*",           .method = HTTP_OPTIONS, .handler = cors_handler },
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    /* WebSocket endpoint */
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        s_ws_count = 0;
    }
    return ESP_OK;
}
```

- [ ] **Step 4: Wire webserver into `main.c`**

Start the webserver after WiFi connects. The simplest approach for Phase 2A: start it unconditionally (it will just not receive requests until WiFi connects). Add to `main/CMakeLists.txt` REQUIRES: `webserver`.

```c
#include "webserver.h"

/* In app_main(), after network_wifi_connect() and ntp_init(): */
webserver_start();
```

Also hook `webserver_notify_clients()` into the state change callback:

```c
static void on_state_change(const status_state_t *new_state, void *user_data)
{
    ui_update(new_state);
    webserver_notify_clients();
}
```

- [ ] **Step 5: Enable WebSocket support in sdkconfig.defaults**

```ini
# HTTP Server
CONFIG_HTTPD_WS_SUPPORT=y
```

- [ ] **Step 6: Build, flash, test REST API**

Test with curl from another machine on the same network:

```bash
curl http://<ESP_IP>/api/status
curl http://<ESP_IP>/api/modes
curl http://<ESP_IP>/api/version
curl -X PUT http://<ESP_IP>/api/status -d '{"mode":1}'
curl -X POST http://<ESP_IP>/api/timer/start -d '{"type":"pomodoro"}'
curl -X POST http://<ESP_IP>/api/timer/stop
```

- [ ] **Step 7: Commit**

```bash
git add components/webserver/ main/ sdkconfig.defaults
git commit -m "feat(webserver): REST API with status CRUD, timer control, WebSocket push"
```

---

## Chunk 4: Thread Safety for API → UI Updates

### Task 4: Marshal State Changes to LVGL Task

**Files:**
- Modify: `main/main.c` — add flag-based marshalling for cross-task state changes

**Problem:** When the REST API (running in the HTTP server task on Core 1) calls `state_set_mode()`, the state change callback calls `ui_update()` which touches LVGL objects. LVGL is single-threaded and must only be called from the LVGL task on Core 0.

**Solution:** Instead of calling `ui_update()` directly from the callback, set a flag. The 1-second `lv_timer` checks the flag and calls `ui_update()` from LVGL context.

- [ ] **Step 1: Add dirty flag pattern in `main.c`**

```c
#include <stdatomic.h>

static atomic_bool s_state_dirty = false;

static void on_state_change(const status_state_t *new_state, void *user_data)
{
    /* Set flag — LVGL timer will pick it up */
    s_state_dirty = true;
    /* WebSocket notify is safe from any task (httpd manages its own locking) */
    webserver_notify_clients();
}

static void one_second_lv_timer_cb(lv_timer_t *timer)
{
    /* Check if state changed from another task */
    if (atomic_exchange(&s_state_dirty, false)) {
        ui_update(state_get());
    }

    state_tick();

    const status_state_t *state = state_get();
    int32_t seconds = state_timer_get_seconds();
    ui_update_timer(seconds, state->timer_type);

    /* Update clock every 30 seconds */
    if (++s_time_update_counter >= 30) {
        s_time_update_counter = 0;
        char time_str[16], date_str[16];
        ntp_get_time_str(time_str, sizeof(time_str));
        ntp_get_date_str(date_str, sizeof(date_str));
        ui_update_time(time_str, date_str);
    }
}
```

Note: `state_tick()` may also trigger `notify_change()` → `on_state_change()`, but since the lv_timer callback is already in LVGL context, the atomic flag will be set and immediately consumed in the same tick. This is safe.

- [ ] **Step 2: Build, flash, test API → display update**

Use curl to change mode, verify the display updates within 1 second.

- [ ] **Step 3: Commit**

```bash
git add main/
git commit -m "fix: thread-safe state→UI marshalling via atomic dirty flag for API calls"
```

---

## Chunk 5: WiFi Status in Top Bar

### Task 5: Show WiFi/IP Status

**Files:**
- Modify: `components/ui/ui_screen.c` — show WiFi indicator in top bar
- Modify: `components/ui/include/ui.h` — add `ui_update_wifi_status()`

- [ ] **Step 1: Add WiFi status update to UI**

In `ui.h`:
```c
void ui_update_wifi_status(const char *ip, bool connected);
```

In `ui_screen.c`, update the weather label (right side of top bar) to show IP when connected:
```c
void ui_update_wifi_status(const char *ip, bool connected)
{
    if (!s_weather_label) return;
    if (connected && ip[0]) {
        lv_label_set_text(s_weather_label, ip);
    } else {
        lv_label_set_text(s_weather_label, "No WiFi");
    }
}
```

- [ ] **Step 2: Update WiFi status periodically in `main.c`**

In the 30-second time update block:
```c
/* Also update WiFi status */
wifi_state_t ws = network_get_state();
ui_update_wifi_status(network_get_ip(), ws == WIFI_STATE_CONNECTED);
```

- [ ] **Step 3: Build, flash, verify IP shows in top bar**

- [ ] **Step 4: Commit**

```bash
git add components/ui/ main/
git commit -m "feat(ui): show WiFi IP address in top bar"
```

---

## Summary: Phase 2A Deliverables

When complete, the tipboard will:

1. **Connect to WiFi** on boot using NVS-stored credentials (AP fallback if none)
2. **Sync time via NTP** and display real clock in the top bar (12-hour format)
3. **Serve a REST API** for remote status control (GET/PUT status, timer start/stop, modes list)
4. **Push state via WebSocket** to connected clients in real-time
5. **Show WiFi IP** in the top bar for easy discovery
6. **Thread-safe** state updates from API → LVGL display via atomic dirty flag

**What's NOT in Phase 2A:**
- Web dashboard HTML/CSS/JS (Phase 2B)
- Weather API (Phase 2C)
- OTA updates (Phase 2C)
- Brightness control via API (Phase 2C)
- WiFi provisioning captive portal web UI (Phase 2B — AP mode exists but no config page yet)

**What's deferred to Phase 2B:**
- `GET /` serving web dashboard from LittleFS
- `GET /api/config` and `PUT /api/config` for device settings
- Dashboard HTML/CSS/JS with WebSocket live updates
- WiFi provisioning web page in AP mode
