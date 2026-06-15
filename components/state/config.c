#include "state.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "config";
static const char *CONFIG_PATH = "/storage/config.json";

/* Default labels */
static const char *DEFAULT_LABELS[MODE_COUNT] = {
    "AVAILABLE", "FOCUSED", "IN A MEETING", "AWAY",
    "POMODORO", "CUSTOM", "ON AIR"
};

static device_config_t s_config;
static bool s_initialized = false;

static void set_defaults(void)
{
    for (int i = 0; i < MODE_COUNT; i++) {
        strncpy(s_config.mode_labels[i], DEFAULT_LABELS[i], sizeof(s_config.mode_labels[i]) - 1);
        s_config.mode_subtitles[i][0] = '\0';
    }
    s_config.default_mode = MODE_AVAILABLE;
    s_config.pomo_work_min = 25;
    s_config.pomo_break_min = 5;
    s_config.brightness = 100;
    s_config.dim_brightness = 15;
    s_config.dim_start_hour = 22;
    s_config.dim_end_hour = 7;
    /* Build-time defaults from .env file (or generic fallbacks) */
    strncpy(s_config.timezone, TIPBOARD_TIMEZONE, sizeof(s_config.timezone) - 1);
    strncpy(s_config.weather_lat, TIPBOARD_WEATHER_LAT, sizeof(s_config.weather_lat) - 1);
    strncpy(s_config.weather_lon, TIPBOARD_WEATHER_LON, sizeof(s_config.weather_lon) - 1);
    strncpy(s_config.mqtt_broker, TIPBOARD_MQTT_BROKER, sizeof(s_config.mqtt_broker) - 1);
    strncpy(s_config.device_name, "tipboard", sizeof(s_config.device_name) - 1);
    s_config.mirror_mode = 0;
    s_config.mirror_source[0] = '\0';
    s_config.mqtt_username[0] = '\0';
    s_config.mqtt_password[0] = '\0';
    s_config.api_token[0] = '\0';
}

static esp_err_t save_config(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *labels = cJSON_CreateArray();
    cJSON *subs = cJSON_CreateArray();
    for (int i = 0; i < MODE_COUNT; i++) {
        cJSON_AddItemToArray(labels, cJSON_CreateString(s_config.mode_labels[i]));
        cJSON_AddItemToArray(subs, cJSON_CreateString(s_config.mode_subtitles[i]));
    }
    cJSON_AddItemToObject(root, "mode_labels", labels);
    cJSON_AddItemToObject(root, "mode_subtitles", subs);
    cJSON_AddNumberToObject(root, "default_mode", s_config.default_mode);
    cJSON_AddNumberToObject(root, "pomo_work_min", s_config.pomo_work_min);
    cJSON_AddNumberToObject(root, "pomo_break_min", s_config.pomo_break_min);
    cJSON_AddNumberToObject(root, "brightness", s_config.brightness);
    cJSON_AddNumberToObject(root, "dim_brightness", s_config.dim_brightness);
    cJSON_AddNumberToObject(root, "dim_start_hour", s_config.dim_start_hour);
    cJSON_AddNumberToObject(root, "dim_end_hour", s_config.dim_end_hour);
    cJSON_AddStringToObject(root, "timezone", s_config.timezone);
    cJSON_AddStringToObject(root, "weather_lat", s_config.weather_lat);
    cJSON_AddStringToObject(root, "weather_lon", s_config.weather_lon);
    cJSON_AddStringToObject(root, "mqtt_broker", s_config.mqtt_broker);
    cJSON_AddStringToObject(root, "device_name", s_config.device_name);
    cJSON_AddNumberToObject(root, "mirror_mode", s_config.mirror_mode);
    cJSON_AddStringToObject(root, "mirror_source", s_config.mirror_source);
    /* Secrets — persisted to LittleFS only, never emitted over HTTP (see webserver). */
    cJSON_AddStringToObject(root, "mqtt_username", s_config.mqtt_username);
    cJSON_AddStringToObject(root, "mqtt_password", s_config.mqtt_password);
    cJSON_AddStringToObject(root, "api_token", s_config.api_token);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) { cJSON_free(str); return ESP_FAIL; }
    fprintf(f, "%s", str);
    fclose(f);
    cJSON_free(str);

    ESP_LOGI(TAG, "Config saved");
    return ESP_OK;
}

static esp_err_t load_config(void)
{
    struct stat st;
    if (stat(CONFIG_PATH, &st) != 0) return ESP_ERR_NOT_FOUND;
    if (st.st_size <= 0 || st.st_size > 4096) {
        ESP_LOGW(TAG, "Config file size %ld out of range, ignoring", (long)st.st_size);
        return ESP_FAIL;
    }

    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return ESP_FAIL;

    char *buf = malloc(st.st_size + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    size_t rd = fread(buf, 1, st.st_size, f);
    fclose(f);
    if (rd != (size_t)st.st_size) {
        ESP_LOGW(TAG, "Config file short read (%zu/%ld), ignoring", rd, (long)st.st_size);
        free(buf);
        return ESP_FAIL;
    }
    buf[rd] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return ESP_FAIL;

    cJSON *labels = cJSON_GetObjectItem(root, "mode_labels");
    if (labels && cJSON_IsArray(labels)) {
        int count = cJSON_GetArraySize(labels);
        if (count > MODE_COUNT) count = MODE_COUNT;
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(labels, i);
            if (item && cJSON_IsString(item)) {
                strncpy(s_config.mode_labels[i], item->valuestring,
                        sizeof(s_config.mode_labels[i]) - 1);
                s_config.mode_labels[i][sizeof(s_config.mode_labels[i]) - 1] = '\0';
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
                strncpy(s_config.mode_subtitles[i], item->valuestring,
                        sizeof(s_config.mode_subtitles[i]) - 1);
                s_config.mode_subtitles[i][sizeof(s_config.mode_subtitles[i]) - 1] = '\0';
            }
        }
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "default_mode"))) s_config.default_mode = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_work_min"))) s_config.pomo_work_min = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_break_min"))) s_config.pomo_break_min = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "brightness"))) s_config.brightness = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_brightness"))) s_config.dim_brightness = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_start_hour"))) s_config.dim_start_hour = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "dim_end_hour"))) s_config.dim_end_hour = item->valueint;
    /* CONFIG_STR: copy a JSON string into a fixed buffer with guaranteed NUL. */
    #define CONFIG_STR(key, field) \
        if ((item = cJSON_GetObjectItem(root, key)) && cJSON_IsString(item)) { \
            strncpy(s_config.field, item->valuestring, sizeof(s_config.field) - 1); \
            s_config.field[sizeof(s_config.field) - 1] = '\0'; \
        }
    CONFIG_STR("timezone", timezone);
    CONFIG_STR("weather_lat", weather_lat);
    CONFIG_STR("weather_lon", weather_lon);
    CONFIG_STR("mqtt_broker", mqtt_broker);
    CONFIG_STR("device_name", device_name);
    if ((item = cJSON_GetObjectItem(root, "mirror_mode")) && cJSON_IsNumber(item))
        s_config.mirror_mode = item->valueint;
    CONFIG_STR("mirror_source", mirror_source);
    CONFIG_STR("mqtt_username", mqtt_username);
    CONFIG_STR("mqtt_password", mqtt_password);
    CONFIG_STR("api_token", api_token);
    #undef CONFIG_STR

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Config loaded");
    return ESP_OK;
}

esp_err_t config_init(void)
{
    set_defaults();
    esp_err_t err = load_config(); /* Override defaults with saved values if they exist */
    if (err == ESP_FAIL) {
        /* Distinguish corruption (parse/size/read fail) from a fresh device
         * (ESP_ERR_NOT_FOUND) — the former silently reverts every setting. */
        ESP_LOGW(TAG, "Saved config invalid/corrupt; running on defaults");
    }
    s_initialized = true;
    return ESP_OK;
}

const device_config_t *config_get(void)
{
    return &s_config;
}

esp_err_t config_set(const device_config_t *cfg)
{
    memcpy(&s_config, cfg, sizeof(s_config));
    return save_config();
}
