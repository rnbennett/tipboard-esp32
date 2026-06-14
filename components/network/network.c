#include "network.h"
#include "state.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"

static const char *TAG = "network";

#define NVS_NAMESPACE     "tipboard"
#define NVS_KEY_SSID      "wifi_ssid"
#define NVS_KEY_PASS      "wifi_pass"
#define AP_SSID            "Tipboard-Setup"
#define MAX_RETRY          10

static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static char s_ip_str[16] = "";
static char s_ssid[33] = "";
static int s_retry_count = 0;
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static esp_timer_handle_t s_reconnect_timer = NULL;
/* Fires off the event-loop task so the reconnect never blocks event dispatch. */
static void reconnect_timer_cb(void *arg) { esp_wifi_connect(); }

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
                /* Schedule the reconnect — never vTaskDelay in the event task,
                 * which would stall all event dispatch (IP events, etc.). */
                if (s_reconnect_timer) {
                    esp_timer_start_once(s_reconnect_timer, 2000000);
                } else {
                    esp_wifi_connect();
                }
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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_wifi_event_group = xEventGroupCreate();

    const esp_timer_create_args_t rc_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect",
    };
    esp_timer_create(&rc_args, &s_reconnect_timer);

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
    strncpy(s_ip_str, "192.168.4.1", sizeof(s_ip_str));
    ESP_LOGI(TAG, "AP mode active at %s", s_ip_str);
}

esp_err_t network_wifi_connect(void)
{
    char ssid[33] = {0};
    char pass[65] = {0};

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

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();

    /* Set hostname from device config */
    const device_config_t *cfg_dev = config_get();
    const char *hostname = (cfg_dev && cfg_dev->device_name[0]) ? cfg_dev->device_name : "tipboard";
    esp_netif_set_hostname(sta, hostname);
    ESP_LOGI(TAG, "Hostname: %s", hostname);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s — continuing without WiFi", esp_err_to_name(err));
        return err;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    /* Degrade gracefully instead of panicking — WiFi runs over the C6 via
     * esp_hosted/SDIO, so these calls can fail transiently. */
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK ||
        (err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config)) != ESP_OK ||
        (err = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(TAG, "WiFi STA start failed: %s", esp_err_to_name(err));
        return err;
    }

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

int network_get_rssi_percent(void)
{
    if (s_state != WIFI_STATE_CONNECTED) return -1;

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) return -1;

    /* Convert RSSI to percentage — same formula as ESPHome:
     * min(max(2 * (rssi + 100), 0), 100) */
    int pct = 2 * (ap_info.rssi + 100);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}
