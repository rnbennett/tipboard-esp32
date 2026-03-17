#include "ntp.h"
#include "state.h"
#include "board.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

static const char *TAG = "ntp";
static bool s_synced = false;

static void time_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
    s_synced = true;
}

esp_err_t ntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_cb;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Timezone from device config */
    const device_config_t *cfg = config_get();
    const char *tz = (cfg && cfg->timezone[0]) ? cfg->timezone : "EST5EDT,M3.2.0,M11.1.0";
    setenv("TZ", tz, 1);
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

    int hour = timeinfo.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char *ampm = timeinfo.tm_hour >= 12 ? "PM" : "AM";
    snprintf(buf, len, "%d:%02d %s", hour, timeinfo.tm_min, ampm);
}

void ntp_get_date_str(char *buf, size_t len)
{
    if (!s_synced || len < 12) {
        buf[0] = '\0';
        return;
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

#if BOARD_DISP_H_RES < 1024
    /* Compact: "3/16" */
    snprintf(buf, len, "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
#else
    /* Full: "Sun Mar 15" */
    strftime(buf, len, "%a %b %d", &timeinfo);
#endif
}
