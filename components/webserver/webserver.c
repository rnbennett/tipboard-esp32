#include "webserver.h"
#include "state.h"
#include "network.h"
#include "board.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

static const char *TAG = "webserver";
static httpd_handle_t s_server = NULL;

/* ── JSON helpers ── */

/* state_to_json() is the shared canonical serializer in the state component. */

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

/* Gate for state-mutating endpoints. If config.api_token is set (non-empty), the
 * request must carry a matching X-Tipboard-Token header; if no token is set the
 * API stays open (unchanged default). Sends 401 and returns false on failure. */
static bool require_auth(httpd_req_t *req)
{
    const device_config_t *cfg = config_get();
    if (!cfg || cfg->api_token[0] == '\0') return true;  /* auth disabled */

    char hdr[40] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Tipboard-Token", hdr, sizeof(hdr)) == ESP_OK
        && strcmp(hdr, cfg->api_token) == 0) {
        return true;
    }
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    return false;
}

/* ── GET /api/status ── */

static esp_err_t api_get_status(httpd_req_t *req)
{
    status_state_t snap;
    state_get_copy(&snap);
    cJSON *json = state_to_json(&snap);
    return send_json_response(req, json);
}

/* ── PUT /api/status ── */

static esp_err_t api_put_status(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    char buf[256];
    if (req->content_len >= (int)sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_FAIL;
    }
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

    /* Subtitle: set if provided, clear if not (default subtitle from config will apply) */
    cJSON *subtitle = cJSON_GetObjectItem(root, "subtitle");
    if (subtitle && cJSON_IsString(subtitle)) {
        state_set_subtitle(subtitle->valuestring);
    } else if (mode) {
        /* Mode changed without explicit subtitle — clear it so config default applies */
        state_set_subtitle("");
    }

    /* Auto-expire with countdown: {"auto_expire_min": 30} */
    cJSON *expire = cJSON_GetObjectItem(root, "auto_expire_min");
    if (expire && cJSON_IsNumber(expire) && expire->valueint > 0 && expire->valueint <= 1440) {
        int secs = expire->valueint * 60;  /* capped at 24h — avoids int32 overflow */
        state_timer_start_countdown(secs);
        state_set_auto_expire(secs, state_get_default_mode());
    }

    cJSON_Delete(root);

    status_state_t snap;
    state_get_copy(&snap);
    cJSON *resp = state_to_json(&snap);
    return send_json_response(req, resp);
}

/* ── POST /api/timer/start ── */

static esp_err_t api_timer_start(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    char buf[128];
    if (req->content_len >= (int)sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_FAIL;
    }
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
            status_state_t s;
            state_get_copy(&s);
            int work_sec = s.pomo_work_sec;
            int break_sec = s.pomo_break_sec;
            cJSON *work_min = cJSON_GetObjectItem(root, "work_min");
            if (work_min && cJSON_IsNumber(work_min)) {
                work_sec = work_min->valueint * 60;
                break_sec = (work_min->valueint >= 25) ? 5 * 60 : 3 * 60;
            }
            state_pomodoro_start(work_sec, break_sec);
        } else if (strcmp(type->valuestring, "elapsed") == 0) {
            state_timer_start_elapsed();
        }
    }

    cJSON_Delete(root);

    status_state_t snap;
    state_get_copy(&snap);
    cJSON *resp = state_to_json(&snap);
    return send_json_response(req, resp);
}

/* ── POST /api/timer/stop ── */

static esp_err_t api_timer_stop(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    status_state_t snap;
    state_get_copy(&snap);
    if (snap.mode == MODE_POMODORO) {
        state_pomodoro_cancel();
    } else {
        state_timer_stop();
    }

    state_get_copy(&snap);
    cJSON *resp = state_to_json(&snap);
    return send_json_response(req, resp);
}

/* ── GET /api/modes ── */

static esp_err_t api_get_modes(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < MODE_COUNT; i++) {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddNumberToObject(m, "id", i);
        cJSON_AddStringToObject(m, "label", state_mode_label(i));
        cJSON_AddItemToArray(root, m);
    }

    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, str);
    cJSON_free(str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── GET /api/config ── */

static esp_err_t api_get_config(httpd_req_t *req)
{
    const device_config_t *cfg = config_get();
    cJSON *root = cJSON_CreateObject();

    cJSON *labels = cJSON_CreateArray();
    cJSON *subs = cJSON_CreateArray();
    for (int i = 0; i < MODE_COUNT; i++) {
        cJSON_AddItemToArray(labels, cJSON_CreateString(cfg->mode_labels[i]));
        cJSON_AddItemToArray(subs, cJSON_CreateString(cfg->mode_subtitles[i]));
    }
    cJSON_AddItemToObject(root, "mode_labels", labels);
    cJSON_AddItemToObject(root, "mode_subtitles", subs);
    cJSON_AddNumberToObject(root, "default_mode", cfg->default_mode);
    cJSON_AddNumberToObject(root, "pomo_work_min", cfg->pomo_work_min);
    cJSON_AddNumberToObject(root, "pomo_break_min", cfg->pomo_break_min);
    cJSON_AddNumberToObject(root, "brightness", cfg->brightness);
    cJSON_AddNumberToObject(root, "dim_brightness", cfg->dim_brightness);
    cJSON_AddNumberToObject(root, "dim_start_hour", cfg->dim_start_hour);
    cJSON_AddNumberToObject(root, "dim_end_hour", cfg->dim_end_hour);
    cJSON_AddStringToObject(root, "timezone", cfg->timezone);
    cJSON_AddStringToObject(root, "weather_lat", cfg->weather_lat);
    cJSON_AddStringToObject(root, "weather_lon", cfg->weather_lon);
    cJSON_AddStringToObject(root, "mqtt_broker", cfg->mqtt_broker);
    cJSON_AddStringToObject(root, "device_name", cfg->device_name);
    cJSON_AddNumberToObject(root, "mirror_mode", cfg->mirror_mode);
    cJSON_AddStringToObject(root, "mirror_source", cfg->mirror_source);
    /* mqtt_username is not a secret; the password and api_token ARE — expose only
     * whether they are set, never the value, so GET /api/config can't leak them. */
    cJSON_AddStringToObject(root, "mqtt_username", cfg->mqtt_username);
    cJSON_AddBoolToObject(root, "mqtt_password_set", cfg->mqtt_password[0] != '\0');
    cJSON_AddBoolToObject(root, "api_token_set", cfg->api_token[0] != '\0');

    return send_json_response(req, root);
}

/* A latitude/longitude string that gets interpolated into the weather API URL.
 * Restrict to numeric characters so it can't inject extra query parameters. */
static bool is_numeric_coord(const char *s)
{
    if (!s || !s[0]) return false;
    for (const char *p = s; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == '+')) return false;
    }
    return true;
}

/* The MQTT broker string is used directly as the client URI — require a known
 * scheme so a config write can't point the device at an arbitrary transport. */
static bool is_valid_broker_uri(const char *s)
{
    return s && (strncmp(s, "mqtt://", 7) == 0 || strncmp(s, "mqtts://", 8) == 0);
}

/* ── PUT /api/config ── */

static esp_err_t api_put_config(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    int total_len = req->content_len;
    if (total_len <= 0 || total_len >= 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            free(buf);
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Receive failed");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t cfg;
    memcpy(&cfg, config_get(), sizeof(cfg));

    cJSON *labels = cJSON_GetObjectItem(root, "mode_labels");
    if (labels && cJSON_IsArray(labels)) {
        int count = cJSON_GetArraySize(labels);
        if (count > MODE_COUNT) count = MODE_COUNT;
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(labels, i);
            if (item && cJSON_IsString(item)) {
                strncpy(cfg.mode_labels[i], item->valuestring,
                        sizeof(cfg.mode_labels[i]) - 1);
                cfg.mode_labels[i][sizeof(cfg.mode_labels[i]) - 1] = '\0';
            }
        }
    }

    cJSON *subs = cJSON_GetObjectItem(root, "mode_subtitles");
    if (subs && cJSON_IsArray(subs)) {
        int count = cJSON_GetArraySize(subs);
        if (count > MODE_COUNT) count = MODE_COUNT;
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(subs, i);
            if (item && cJSON_IsString(item)) {
                strncpy(cfg.mode_subtitles[i], item->valuestring,
                        sizeof(cfg.mode_subtitles[i]) - 1);
                cfg.mode_subtitles[i][sizeof(cfg.mode_subtitles[i]) - 1] = '\0';
            }
        }
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "default_mode")) && cJSON_IsNumber(item))
        cfg.default_mode = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_work_min")) && cJSON_IsNumber(item))
        cfg.pomo_work_min = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_break_min")) && cJSON_IsNumber(item))
        cfg.pomo_break_min = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "brightness")) && cJSON_IsNumber(item))
        cfg.brightness = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_brightness")) && cJSON_IsNumber(item))
        cfg.dim_brightness = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_start_hour")) && cJSON_IsNumber(item))
        cfg.dim_start_hour = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_end_hour")) && cJSON_IsNumber(item))
        cfg.dim_end_hour = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "timezone")) && cJSON_IsString(item)) {
        strncpy(cfg.timezone, item->valuestring, sizeof(cfg.timezone) - 1);
        cfg.timezone[sizeof(cfg.timezone) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "weather_lat")) && cJSON_IsString(item)) {
        if (!is_numeric_coord(item->valuestring)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid latitude");
            return ESP_FAIL;
        }
        strncpy(cfg.weather_lat, item->valuestring, sizeof(cfg.weather_lat) - 1);
        cfg.weather_lat[sizeof(cfg.weather_lat) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "weather_lon")) && cJSON_IsString(item)) {
        if (!is_numeric_coord(item->valuestring)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid longitude");
            return ESP_FAIL;
        }
        strncpy(cfg.weather_lon, item->valuestring, sizeof(cfg.weather_lon) - 1);
        cfg.weather_lon[sizeof(cfg.weather_lon) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "mqtt_broker")) && cJSON_IsString(item)) {
        if (!is_valid_broker_uri(item->valuestring)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Broker must be mqtt:// or mqtts://");
            return ESP_FAIL;
        }
        strncpy(cfg.mqtt_broker, item->valuestring, sizeof(cfg.mqtt_broker) - 1);
        cfg.mqtt_broker[sizeof(cfg.mqtt_broker) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "device_name")) && cJSON_IsString(item)) {
        strncpy(cfg.device_name, item->valuestring, sizeof(cfg.device_name) - 1);
        cfg.device_name[sizeof(cfg.device_name) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "mirror_mode")) && cJSON_IsNumber(item))
        cfg.mirror_mode = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "mirror_source")) && cJSON_IsString(item)) {
        strncpy(cfg.mirror_source, item->valuestring, sizeof(cfg.mirror_source) - 1);
        cfg.mirror_source[sizeof(cfg.mirror_source) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "mqtt_username")) && cJSON_IsString(item)) {
        strncpy(cfg.mqtt_username, item->valuestring, sizeof(cfg.mqtt_username) - 1);
        cfg.mqtt_username[sizeof(cfg.mqtt_username) - 1] = '\0';
    }
    /* Secrets: only overwrite when a non-empty value is supplied, so a config
     * round-trip (GET returns no value, PUT omits/blank) doesn't wipe them. */
    if ((item = cJSON_GetObjectItem(root, "mqtt_password")) && cJSON_IsString(item) && item->valuestring[0]) {
        strncpy(cfg.mqtt_password, item->valuestring, sizeof(cfg.mqtt_password) - 1);
        cfg.mqtt_password[sizeof(cfg.mqtt_password) - 1] = '\0';
    }
    if ((item = cJSON_GetObjectItem(root, "api_token")) && cJSON_IsString(item) && item->valuestring[0]) {
        strncpy(cfg.api_token, item->valuestring, sizeof(cfg.api_token) - 1);
        cfg.api_token[sizeof(cfg.api_token) - 1] = '\0';
    }

    cJSON_Delete(root);
    config_set(&cfg);

    /* Notify UI so mirror mode indicator and other config-driven UI updates immediately */
    state_notify_change();

    return api_get_config(req);
}

/* ── GET /api/version ── */

static esp_err_t api_get_version(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", "1.0.0-phase2a");
    cJSON_AddStringToObject(root, "api", "1.0");
    cJSON_AddStringToObject(root, "board", "JC1060P470C");
    cJSON_AddStringToObject(root, "ip", network_get_ip());
    return send_json_response(req, root);
}

/* ── PUT /api/brightness ── */

static esp_err_t api_put_brightness(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (val && cJSON_IsNumber(val)) {
        int brightness = val->valueint;
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;
        board_backlight_set(brightness);
        ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    }

    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    return send_json_response(req, resp);
}

/* ── WebSocket /ws ── */

#define MAX_WS_CLIENTS 4
static int s_ws_fds[MAX_WS_CLIENTS];
static int s_ws_count = 0;
static SemaphoreHandle_t s_ws_lock = NULL;   /* guards s_ws_fds[]/s_ws_count */

static void ws_remove_fd_locked(int fd)
{
    int n = 0;
    for (int i = 0; i < s_ws_count; i++) {
        if (s_ws_fds[i] != fd) s_ws_fds[n++] = s_ws_fds[i];
    }
    s_ws_count = n;
}

/* httpd invokes this whenever it closes/purges a socket (LRU purge, keep-alive
 * timeout, client close). Prune our tracking array, then do the real close —
 * installing a custom close_fn overrides httpd's own socket close. */
static void ws_close_fn(httpd_handle_t hd, int sockfd)
{
    if (s_ws_lock) xSemaphoreTake(s_ws_lock, portMAX_DELAY);
    ws_remove_fd_locked(sockfd);
    if (s_ws_lock) xSemaphoreGive(s_ws_lock);
    close(sockfd);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        if (s_ws_lock) xSemaphoreTake(s_ws_lock, portMAX_DELAY);
        ws_remove_fd_locked(fd);   /* dedup: drop any stale entry for this fd */
        if (s_ws_count < MAX_WS_CLIENTS) {
            s_ws_fds[s_ws_count++] = fd;
            ESP_LOGI(TAG, "WebSocket client connected (fd=%d, total=%d)", fd, s_ws_count);
        } else {
            ESP_LOGW(TAG, "WebSocket client rejected (fd=%d): max clients", fd);
        }
        if (s_ws_lock) xSemaphoreGive(s_ws_lock);
        return ESP_OK;
    }

    /* Receive and discard incoming WS frames */
    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    /* Bound the client-declared frame size before allocating — these frames are
     * only ever discarded, so cap at 4 KB to stop a malicious client forcing a
     * large heap allocation (and to avoid a size_t+1 wrap on a huge value). */
    if (ws_pkt.len > 4096) {
        ESP_LOGW(TAG, "WS frame too large (%u), ignoring", (unsigned)ws_pkt.len);
        return ESP_OK;
    }
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
    /* s_ws_count is mutated under s_ws_lock by the httpd task; don't read it
     * unlocked here. The locked send loop below naturally no-ops with 0 clients. */
    if (!s_server) return;

    status_state_t snap;
    state_get_copy(&snap);
    cJSON *json = state_to_json(&snap);
    char *str = cJSON_PrintUnformatted(json);

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)str,
        .len = strlen(str),
    };

    if (s_ws_lock) xSemaphoreTake(s_ws_lock, portMAX_DELAY);
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
    if (s_ws_lock) xSemaphoreGive(s_ws_lock);

    cJSON_free(str);
    cJSON_Delete(json);
}

/* ── POST /api/ota — firmware update ── */

static esp_err_t api_ota_handler(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    /* Reject a missing/zero/negative Content-Length before touching the OTA
     * partition: with content_len <= 0 the write loop below runs zero times and
     * esp_ota_end() would commit an empty image — bricking the device. Also cap
     * to the partition size so a bogus length can't be trusted downstream. */
    if (req->content_len <= 0 || (size_t)req->content_len > update_partition->size) {
        ESP_LOGE(TAG, "OTA rejected: invalid content_len %ld (partition %lu)",
                 (long)req->content_len, (unsigned long)update_partition->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware size");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA: writing to partition '%s' at 0x%lx (%ld bytes incoming)",
             update_partition->label, update_partition->address, (long)req->content_len);

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, (remaining < 4096) ? remaining : 4096);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "OTA receive error at %d bytes", received);
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }

        /* First byte of an ESP32 app image is the magic 0xE9. Reject anything
         * else up front (esp_ota_end would catch it later, but this fails fast
         * and avoids writing a junk image to the inactive partition). */
        if (received == 0 && (uint8_t)buf[0] != 0xE9) {
            ESP_LOGE(TAG, "OTA rejected: bad image magic 0x%02x", (uint8_t)buf[0]);
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not a valid firmware image");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }

        remaining -= recv_len;
        received += recv_len;
    }

    free(buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA: success! Rebooting in 2 seconds...");

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "message", "Firmware updated. Rebooting...");
    send_json_response(req, resp);

    /* Delay then reboot */
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK; /* never reached */
}

/* ── GET / — serve web dashboard ── */

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t setup_html_start[] asm("_binary_setup_html_start");
extern const uint8_t setup_html_end[]   asm("_binary_setup_html_end");

static esp_err_t dashboard_handler(httpd_req_t *req)
{
    /* Serve setup page in AP mode, dashboard in STA mode */
    wifi_state_t ws = network_get_state();
    if (ws == WIFI_STATE_AP_MODE) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, (const char *)setup_html_start,
                        setup_html_end - setup_html_start);
    } else {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, (const char *)index_html_start,
                        index_html_end - index_html_start);
    }
    return ESP_OK;
}

/* ── POST /api/wifi — save WiFi credentials and reboot ── */

static esp_err_t api_wifi_handler(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "password");

    if (!ssid || !cJSON_IsString(ssid) || !ssid->valuestring[0]) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    network_set_credentials(ssid->valuestring,
                            (pass && cJSON_IsString(pass)) ? pass->valuestring : "");
    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    send_json_response(req, resp);

    ESP_LOGI(TAG, "WiFi credentials saved. Rebooting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

/* ── POST /api/reboot — restart the device ── */

static esp_err_t api_reboot_handler(httpd_req_t *req)
{
    if (!require_auth(req)) return ESP_FAIL;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "message", "Rebooting in 2 seconds...");
    send_json_response(req, resp);

    ESP_LOGI(TAG, "Reboot requested via API");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
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
    if (!s_ws_lock) s_ws_lock = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;   /* evict stalest socket instead of refusing new connections */
    config.close_fn = ws_close_fn;    /* keep s_ws_fds[] in sync on every socket close */

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t uris[] = {
        { .uri = "/api/status",      .method = HTTP_GET,     .handler = api_get_status },
        { .uri = "/api/status",      .method = HTTP_PUT,     .handler = api_put_status },
        { .uri = "/api/timer/start", .method = HTTP_POST,    .handler = api_timer_start },
        { .uri = "/api/timer/stop",  .method = HTTP_POST,    .handler = api_timer_stop },
        { .uri = "/api/modes",       .method = HTTP_GET,     .handler = api_get_modes },
        { .uri = "/api/version",     .method = HTTP_GET,     .handler = api_get_version },
        { .uri = "/api/config",      .method = HTTP_GET,     .handler = api_get_config },
        { .uri = "/api/config",      .method = HTTP_PUT,     .handler = api_put_config },
        { .uri = "/api/brightness",  .method = HTTP_PUT,     .handler = api_put_brightness },
        { .uri = "/api/ota",         .method = HTTP_POST,    .handler = api_ota_handler },
        { .uri = "/api/wifi",        .method = HTTP_POST,    .handler = api_wifi_handler },
        { .uri = "/api/reboot",     .method = HTTP_POST,    .handler = api_reboot_handler },
        { .uri = "/api/*",           .method = HTTP_OPTIONS, .handler = cors_handler },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    /* Dashboard — must be last (wildcard catches all non-API routes) */
    httpd_uri_t dashboard_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = dashboard_handler,
    };
    httpd_register_uri_handler(s_server, &dashboard_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        if (s_ws_lock) xSemaphoreTake(s_ws_lock, portMAX_DELAY);
        s_ws_count = 0;
        if (s_ws_lock) xSemaphoreGive(s_ws_lock);
    }
    return ESP_OK;
}
