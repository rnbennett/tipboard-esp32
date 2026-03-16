#include "tipboard_mqtt.h"
#include "state.h"
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

static const char *TAG = "mqtt";

/* Broker URI fallback if config is empty */
#define MQTT_BROKER_DEFAULT   ""
#define TOPIC_STATUS      "tipboard/status"
#define TOPIC_COMMAND     "tipboard/command"
#define TOPIC_CALENDAR    "tipboard/calendar"
#define TOPIC_AVAILABLE   "tipboard/available"

/* HA auto-discovery topics */
#define HA_SENSOR_CONFIG  "homeassistant/sensor/tipboard_status/config"
#define HA_SELECT_CONFIG  "homeassistant/select/tipboard_mode/config"

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

/* ── Build state JSON (same format as REST API) ── */

static char *state_to_json_str(void)
{
    const status_state_t *state = state_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", state_mode_label(state->mode));
    cJSON_AddNumberToObject(root, "mode_id", state->mode);
    cJSON_AddStringToObject(root, "subtitle", state->subtitle);
    cJSON_AddNumberToObject(root, "timer_type", state->timer_type);
    cJSON_AddNumberToObject(root, "timer_seconds", state_timer_get_seconds());
    cJSON_AddNumberToObject(root, "pomo_phase", state->pomo_phase);
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

/* ── HA Auto-Discovery ── */

static void publish_ha_discovery(void)
{
    /* Sensor entity — shows current mode as text */
    cJSON *sensor = cJSON_CreateObject();
    cJSON_AddStringToObject(sensor, "name", "Tipboard Status");
    cJSON_AddStringToObject(sensor, "unique_id", "tipboard_status");
    cJSON_AddStringToObject(sensor, "state_topic", TOPIC_STATUS);
    cJSON_AddStringToObject(sensor, "value_template", "{{ value_json.mode }}");
    cJSON_AddStringToObject(sensor, "icon", "mdi:desk");
    cJSON_AddStringToObject(sensor, "availability_topic", TOPIC_AVAILABLE);

    cJSON *dev = cJSON_CreateObject();
    cJSON_AddStringToObject(dev, "name", "Tipboard");
    cJSON_AddStringToObject(dev, "manufacturer", "DIY");
    cJSON_AddStringToObject(dev, "model", "JC1060P470C ESP32-P4");
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString("tipboard_01"));
    cJSON_AddItemToObject(dev, "identifiers", ids);
    cJSON_AddItemToObject(sensor, "device", dev);

    char *sensor_str = cJSON_PrintUnformatted(sensor);
    esp_mqtt_client_publish(s_client, HA_SENSOR_CONFIG, sensor_str, 0, 1, 1);
    cJSON_free(sensor_str);
    cJSON_Delete(sensor);

    /* Select entity — allows mode switching from HA */
    cJSON *sel = cJSON_CreateObject();
    cJSON_AddStringToObject(sel, "name", "Tipboard Mode");
    cJSON_AddStringToObject(sel, "unique_id", "tipboard_mode");
    cJSON_AddStringToObject(sel, "command_topic", TOPIC_COMMAND);
    cJSON_AddStringToObject(sel, "command_template", "{\"mode_name\":\"{{ value }}\"}");
    cJSON_AddStringToObject(sel, "state_topic", TOPIC_STATUS);
    cJSON_AddStringToObject(sel, "value_template", "{{ value_json.mode }}");
    cJSON_AddStringToObject(sel, "icon", "mdi:traffic-light");
    cJSON_AddStringToObject(sel, "availability_topic", TOPIC_AVAILABLE);

    cJSON *options = cJSON_CreateArray();
    for (int i = 0; i < MODE_COUNT; i++) {
        cJSON_AddItemToArray(options, cJSON_CreateString(state_mode_label(i)));
    }
    cJSON_AddItemToObject(sel, "options", options);

    cJSON *dev2 = cJSON_CreateObject();
    cJSON_AddStringToObject(dev2, "name", "Tipboard");
    cJSON_AddStringToObject(dev2, "manufacturer", "DIY");
    cJSON_AddStringToObject(dev2, "model", "JC1060P470C ESP32-P4");
    cJSON *ids2 = cJSON_CreateArray();
    cJSON_AddItemToArray(ids2, cJSON_CreateString("tipboard_01"));
    cJSON_AddItemToObject(dev2, "identifiers", ids2);
    cJSON_AddItemToObject(sel, "device", dev2);

    char *sel_str = cJSON_PrintUnformatted(sel);
    esp_mqtt_client_publish(s_client, HA_SELECT_CONFIG, sel_str, 0, 1, 1);
    cJSON_free(sel_str);
    cJSON_Delete(sel);

    ESP_LOGI(TAG, "HA auto-discovery published");
}

/* ── Handle incoming MQTT command ── */

static void handle_command(const char *data, int len)
{
    char *buf = malloc(len + 1);
    if (!buf) return;
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "Invalid command JSON");
        return;
    }

    /* Mode by ID: {"mode": 1} */
    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (mode && cJSON_IsNumber(mode)) {
        int id = mode->valueint;
        if (id >= 0 && id < MODE_COUNT) {
            state_set_mode((status_mode_t)id, SOURCE_MQTT);
        }
    }

    /* Mode by name: {"mode_name": "FOCUSED"} — for HA select entity */
    cJSON *mode_name = cJSON_GetObjectItem(root, "mode_name");
    if (mode_name && cJSON_IsString(mode_name)) {
        for (int i = 0; i < MODE_COUNT; i++) {
            if (strcasecmp(mode_name->valuestring, state_mode_label(i)) == 0) {
                state_set_mode((status_mode_t)i, SOURCE_MQTT);
                break;
            }
        }
    }

    /* Subtitle: set if provided, clear if mode changed without one */
    cJSON *subtitle = cJSON_GetObjectItem(root, "subtitle");
    if (subtitle && cJSON_IsString(subtitle)) {
        state_set_subtitle(subtitle->valuestring);
    } else if (mode || mode_name) {
        state_set_subtitle("");
    }

    /* Auto-expire with countdown: {"auto_expire_min": 30} */
    cJSON *expire = cJSON_GetObjectItem(root, "auto_expire_min");
    if (expire && cJSON_IsNumber(expire) && expire->valueint > 0) {
        int secs = expire->valueint * 60;
        state_timer_start_countdown(secs);
        state_set_auto_expire(secs, state_get_default_mode());
    }

    /* Timer commands: {"command": "timer_start", "type": "pomodoro"} */
    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    if (cmd && cJSON_IsString(cmd)) {
        if (strcmp(cmd->valuestring, "timer_start") == 0) {
            cJSON *type = cJSON_GetObjectItem(root, "type");
            if (type && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "pomodoro") == 0) {
                    const status_state_t *s = state_get();
                    state_pomodoro_start(s->pomo_work_sec, s->pomo_break_sec);
                } else if (strcmp(type->valuestring, "elapsed") == 0) {
                    state_timer_start_elapsed();
                }
            }
        } else if (strcmp(cmd->valuestring, "timer_stop") == 0) {
            const status_state_t *s = state_get();
            if (s->mode == MODE_POMODORO) {
                state_pomodoro_cancel();
            } else {
                state_timer_stop();
            }
        }
    }

    /* Brightness: {"command": "brightness", "value": 50} */
    if (cmd && cJSON_IsString(cmd) && strcmp(cmd->valuestring, "brightness") == 0) {
        cJSON *val = cJSON_GetObjectItem(root, "value");
        if (val && cJSON_IsNumber(val)) {
            int brightness = val->valueint;
            if (brightness < 0) brightness = 0;
            if (brightness > 100) brightness = 100;
            extern esp_err_t board_backlight_set(uint8_t brightness_pct);
            board_backlight_set((uint8_t)brightness);
        }
    }

    cJSON_Delete(root);
}

/* ── Handle incoming calendar event ── */

/* Forward declare UI function */
extern void ui_update_calendar(const char *title, const char *time_str);

static void handle_calendar(const char *data, int len)
{
    char *buf = malloc(len + 1);
    if (!buf) return;
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    cJSON *title = cJSON_GetObjectItem(root, "title");
    cJSON *start = cJSON_GetObjectItem(root, "start");

    const char *title_str = (title && cJSON_IsString(title)) ? title->valuestring : "";
    const char *start_str = (start && cJSON_IsString(start)) ? start->valuestring : "";

    /* Filter out fake/blocked calendar events (start with lightning bolt) */
    if (title_str[0] != '\0') {
        /* Check for UTF-8 lightning bolt (U+26A1 = 0xE2 0x9A 0xA1) or
         * lock symbol (U+1F512) at the start, or "Friday-managed" in text */
        /* Filter fake events: start with ⚡ (UTF-8: 0xE2 0x9A 0xA1) or contain Friday-managed */
        if ((unsigned char)title_str[0] == 0xE2 &&
            (unsigned char)title_str[1] == 0x9A &&
            (unsigned char)title_str[2] == 0xA1) {
            ESP_LOGI(TAG, "Calendar: skipping fake meeting (⚡): %s", title_str);
            title_str = "";
        } else if (strstr(title_str, "Friday-managed") != NULL ||
                   strstr(title_str, "actually free") != NULL) {
            ESP_LOGI(TAG, "Calendar: skipping fake meeting: %s", title_str);
            title_str = "";
        }
    }

    /* Extract time from ISO start string "2026-03-15T14:00:00" → "2:00 PM" */
    char time_display[16] = "";
    if (start_str[0] && strlen(start_str) >= 16) {
        int hour = 0, min = 0;
        if (sscanf(start_str + 11, "%d:%d", &hour, &min) == 2) {
            int h12 = hour % 12;
            if (h12 == 0) h12 = 12;
            const char *ampm = hour >= 12 ? "PM" : "AM";
            snprintf(time_display, sizeof(time_display), "%d:%02d %s", h12, min, ampm);
        }
    }

    ESP_LOGI(TAG, "Calendar: \"%s\" at %s", title_str, time_display);
    ui_update_calendar(title_str, time_display);

    cJSON_Delete(root);
}

/* ── MQTT event handler ── */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        s_connected = true;

        /* Publish online status */
        esp_mqtt_client_publish(s_client, TOPIC_AVAILABLE, "online", 0, 1, 1);

        /* Subscribe to command and calendar topics */
        esp_mqtt_client_subscribe(s_client, TOPIC_COMMAND, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_CALENDAR, 1);

        /* Publish HA auto-discovery */
        publish_ha_discovery();

        /* Publish current state */
        tipboard_mqtt_publish_state();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_connected = false;
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len > 0) {
            if (strncmp(event->topic, TOPIC_COMMAND, event->topic_len) == 0) {
                handle_command(event->data, event->data_len);
            } else if (strncmp(event->topic, TOPIC_CALENDAR, event->topic_len) == 0) {
                handle_calendar(event->data, event->data_len);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ── Public API ── */

esp_err_t tipboard_mqtt_init(void)
{
    const device_config_t *cfg = config_get();
    const char *broker = (cfg && cfg->mqtt_broker[0]) ? cfg->mqtt_broker : MQTT_BROKER_DEFAULT;
    if (!broker[0]) {
        ESP_LOGW(TAG, "No MQTT broker configured — skipping MQTT");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Connecting to MQTT broker: %s", broker);

    esp_mqtt_client_config_t config = {
        .broker.address.uri = broker,
        .network.reconnect_timeout_ms = 5000,
        .session.last_will = {
            .topic = TOPIC_AVAILABLE,
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = 1,
        },
    };

    s_client = esp_mqtt_client_init(&config);
    if (!s_client) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    return ESP_OK;
}

void tipboard_mqtt_publish_state(void)
{
    if (!s_connected || !s_client) return;

    char *json = state_to_json_str();
    if (json) {
        esp_mqtt_client_publish(s_client, TOPIC_STATUS, json, 0, 0, 1);
        cJSON_free(json);
    }
}
