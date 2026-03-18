#include "persist.h"
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_littlefs.h"
#include "cJSON.h"
#include "esp_timer.h"

static const char *TAG = "persist";
static const char *STATE_PATH = "/storage/state.json";

static int64_t s_last_save_us = 0;
static const int64_t DEBOUNCE_US = 2000000LL; /* 2 seconds */
static bool s_mounted = false;

esp_err_t persist_init(void)
{
    if (s_mounted) return ESP_OK;  /* Already mounted — idempotent */

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(err));
        return err;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    ESP_LOGI(TAG, "LittleFS mounted: %zu/%zu bytes used", used, total);

    s_mounted = true;
    return ESP_OK;
}

esp_err_t persist_save_now(const status_state_t *state)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "mode", state->mode);
    cJSON_AddStringToObject(root, "subtitle", state->subtitle);
    cJSON_AddNumberToObject(root, "timer_type", state->timer_type);
    cJSON_AddNumberToObject(root, "timer_started_at", (double)state->timer_started_at);
    cJSON_AddNumberToObject(root, "timer_duration_sec", state->timer_duration_sec);
    cJSON_AddBoolToObject(root, "auto_expire_enabled", state->auto_expire_enabled);
    cJSON_AddNumberToObject(root, "auto_expire_at", (double)state->auto_expire_at);
    cJSON_AddNumberToObject(root, "auto_expire_revert_to", state->auto_expire_revert_to);
    cJSON_AddNumberToObject(root, "source", state->source);
    cJSON_AddNumberToObject(root, "priority", state->priority);
    cJSON_AddNumberToObject(root, "pomo_phase", state->pomo_phase);
    cJSON_AddNumberToObject(root, "pomo_work_sec", state->pomo_work_sec);
    cJSON_AddNumberToObject(root, "pomo_break_sec", state->pomo_break_sec);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(STATE_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", STATE_PATH);
        cJSON_free(json_str);
        return ESP_FAIL;
    }

    fprintf(f, "%s", json_str);
    fclose(f);
    cJSON_free(json_str);

    s_last_save_us = esp_timer_get_time();
    ESP_LOGD(TAG, "State saved to %s", STATE_PATH);
    return ESP_OK;
}

void persist_save_debounced(const status_state_t *state)
{
    if (!s_mounted) return;

    int64_t now = esp_timer_get_time();
    if ((now - s_last_save_us) < DEBOUNCE_US) {
        return;
    }

    persist_save_now(state);
}

esp_err_t persist_load(status_state_t *state)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    struct stat st;
    if (stat(STATE_PATH, &st) != 0) {
        ESP_LOGI(TAG, "No saved state found");
        return ESP_ERR_NOT_FOUND;
    }

    FILE *f = fopen(STATE_PATH, "r");
    if (!f) return ESP_FAIL;

    char *buf = malloc(st.st_size + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }

    size_t rd = fread(buf, 1, st.st_size, f);
    fclose(f);
    buf[rd] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse saved state JSON");
        return ESP_FAIL;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "mode"))) state->mode = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "subtitle"))) {
        strncpy(state->subtitle, item->valuestring, sizeof(state->subtitle) - 1);
    }
    if ((item = cJSON_GetObjectItem(root, "timer_type"))) state->timer_type = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "timer_started_at"))) state->timer_started_at = (int64_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "timer_duration_sec"))) state->timer_duration_sec = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "auto_expire_enabled"))) state->auto_expire_enabled = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(root, "auto_expire_at"))) state->auto_expire_at = (int64_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "auto_expire_revert_to"))) state->auto_expire_revert_to = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "source"))) state->source = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "priority"))) state->priority = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_phase"))) state->pomo_phase = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_work_sec"))) state->pomo_work_sec = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_break_sec"))) state->pomo_break_sec = item->valueint;

    cJSON_Delete(root);
    ESP_LOGI(TAG, "State loaded from %s", STATE_PATH);
    return ESP_OK;
}
