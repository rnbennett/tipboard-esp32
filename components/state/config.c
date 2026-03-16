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

    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return ESP_FAIL;

    char *buf = malloc(st.st_size + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    size_t rd = fread(buf, 1, st.st_size, f);
    fclose(f);
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
    if ((item = cJSON_GetObjectItem(root, "timezone")) && cJSON_IsString(item))
        strncpy(s_config.timezone, item->valuestring, sizeof(s_config.timezone) - 1);
    if ((item = cJSON_GetObjectItem(root, "weather_lat")) && cJSON_IsString(item))
        strncpy(s_config.weather_lat, item->valuestring, sizeof(s_config.weather_lat) - 1);
    if ((item = cJSON_GetObjectItem(root, "weather_lon")) && cJSON_IsString(item))
        strncpy(s_config.weather_lon, item->valuestring, sizeof(s_config.weather_lon) - 1);
    if ((item = cJSON_GetObjectItem(root, "mqtt_broker")) && cJSON_IsString(item))
        strncpy(s_config.mqtt_broker, item->valuestring, sizeof(s_config.mqtt_broker) - 1);
    if ((item = cJSON_GetObjectItem(root, "device_name")) && cJSON_IsString(item))
        strncpy(s_config.device_name, item->valuestring, sizeof(s_config.device_name) - 1);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Config loaded");
    return ESP_OK;
}

esp_err_t config_init(void)
{
    set_defaults();
    load_config(); /* Override defaults with saved values if they exist */
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
