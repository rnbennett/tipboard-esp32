#include "weather.h"
#include "state.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "weather";

#define FETCH_INTERVAL_MS  (15 * 60 * 1000)  /* 15 minutes */
#define HTTP_BUF_SIZE      1024

static weather_data_t s_weather = {0};

/* WMO weather code descriptions */
const char *weather_code_desc(int code)
{
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mostly Clear";
    case 2:  return "Partly Cloudy";
    case 3:  return "Overcast";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 66: case 67: return "Freezing Rain";
    case 71: case 73: case 75: return "Snow";
    case 77: return "Snow Grains";
    case 80: case 81: case 82: return "Showers";
    case 85: case 86: return "Snow Showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "T-Storm + Hail";
    default: return "Unknown";
    }
}

/* FontAwesome weather icon glyphs (merged into font_prototype_20) */
#define ICON_SUN          "\xEF\x86\x85"   /* U+F185 */
#define ICON_CLOUD        "\xEF\x83\x82"   /* U+F0C2 */
#define ICON_CLOUD_SUN    "\xEF\x9B\x84"   /* U+F6C4 */
#define ICON_CLOUD_RAIN   "\xEF\x9C\xBD"   /* U+F73D */
#define ICON_SHOWERS      "\xEF\x9D\x80"   /* U+F740 */
#define ICON_SNOWFLAKE    "\xEF\x8B\x9C"   /* U+F2DC */
#define ICON_BOLT         "\xEF\x83\xA7"   /* U+F0E7 */
#define ICON_SMOG         "\xEF\x9D\x9F"   /* U+F75F */

const char *weather_code_icon(int code)
{
    if (code == 0) return ICON_SUN;                          /* Clear sky */
    if (code == 1) return ICON_CLOUD_SUN;                    /* Mostly clear */
    if (code <= 3) return ICON_CLOUD;                        /* Partly cloudy / Overcast */
    if (code <= 48) return ICON_SMOG;                        /* Fog */
    if (code <= 57) return ICON_CLOUD_RAIN;                  /* Drizzle */
    if (code <= 67) return ICON_CLOUD_RAIN;                  /* Rain / Freezing rain */
    if (code <= 77) return ICON_SNOWFLAKE;                   /* Snow */
    if (code <= 82) return ICON_SHOWERS;                     /* Showers */
    if (code <= 86) return ICON_SNOWFLAKE;                   /* Snow showers */
    if (code >= 95) return ICON_BOLT;                        /* Thunderstorm */
    return ICON_CLOUD;
}

static esp_err_t fetch_weather(void)
{
    const device_config_t *cfg = config_get();
    const char *lat = (cfg && cfg->weather_lat[0]) ? cfg->weather_lat : "0.0000";
    const char *lon = (cfg && cfg->weather_lon[0]) ? cfg->weather_lon : "0.0000";

    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%s&longitude=%s"
             "&current=temperature_2m,weather_code,relative_humidity_2m,precipitation_probability"
             "&temperature_unit=fahrenheit&timezone=America/New_York",
             lat, lon);

    char *buf = malloc(HTTP_BUF_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(buf); return ESP_FAIL; }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buf);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int read_len = esp_http_client_read(client, buf, HTTP_BUF_SIZE - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len <= 0) {
        ESP_LOGE(TAG, "HTTP read failed (len=%d, content_length=%d)", read_len, content_length);
        free(buf);
        return ESP_FAIL;
    }
    buf[read_len] = '\0';

    /* Parse JSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return ESP_FAIL;
    }

    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (current) {
        cJSON *temp = cJSON_GetObjectItem(current, "temperature_2m");
        cJSON *wcode = cJSON_GetObjectItem(current, "weather_code");
        cJSON *humid = cJSON_GetObjectItem(current, "relative_humidity_2m");
        cJSON *precip = cJSON_GetObjectItem(current, "precipitation_probability");

        if (temp) s_weather.temp_f = (float)temp->valuedouble;
        if (wcode) s_weather.weather_code = wcode->valueint;
        if (humid) s_weather.humidity = humid->valueint;
        if (precip) s_weather.precip_chance = precip->valueint;
        s_weather.valid = true;

        ESP_LOGI(TAG, "Weather: %.0f°F %s (%d) humidity=%d%% precip=%d%%",
                 s_weather.temp_f, weather_code_desc(s_weather.weather_code),
                 s_weather.weather_code, s_weather.humidity, s_weather.precip_chance);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static void weather_task(void *arg)
{
    /* Wait for WiFi to connect before first fetch */
    vTaskDelay(pdMS_TO_TICKS(15000));

    while (1) {
        fetch_weather();
        vTaskDelay(pdMS_TO_TICKS(FETCH_INTERVAL_MS));
    }
}

esp_err_t weather_init(void)
{
    ESP_LOGI(TAG, "Starting weather polling (every 15 min)");
    xTaskCreatePinnedToCore(weather_task, "weather", 8192, NULL, 3, NULL, 1);
    return ESP_OK;
}

const weather_data_t *weather_get(void)
{
    return &s_weather;
}
