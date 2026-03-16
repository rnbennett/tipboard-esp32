#include <stdio.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "board.h"
#include "state.h"
#include "ui.h"
#include "network.h"
#include "ntp.h"
#include "webserver.h"
#include "weather.h"
#include "tipboard_mqtt.h"
#include <time.h>

static const char *TAG = "tipboard";

static esp_lcd_touch_handle_t s_touch = NULL;

/* Thread-safe state→UI marshalling: API calls on Core 1 set this flag,
 * the LVGL timer on Core 0 picks it up and calls ui_update(). */
static atomic_bool s_state_dirty = false;
static int s_time_update_counter = 0;

/* ── LVGL display flush callback ── */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel,
        area->x1, area->y1,
        area->x2 + 1, area->y2 + 1,
        px_map);
    lv_display_flush_ready(disp);
}

/* ── LVGL touch read callback ── */
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (s_touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    esp_lcd_touch_read_data(s_touch);

    esp_lcd_touch_point_data_t tp_data[1];
    uint8_t count = 0;
    esp_lcd_touch_get_data(s_touch, tp_data, &count, 1);

    if (count > 0) {
        data->point.x = tp_data[0].x;
        data->point.y = tp_data[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ── LVGL tick callback (2ms) ── */
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(2);
}

/* ── State change callback — may run from any task ── */
static void on_state_change(const status_state_t *new_state, void *user_data)
{
    s_state_dirty = true;
    webserver_notify_clients();
    tipboard_mqtt_publish_state();
}

/* ── 1-second LVGL timer: state tick + timer + time + WiFi display ── */
static void one_second_lv_timer_cb(lv_timer_t *timer)
{
    /* Pick up state changes from API/MQTT (cross-task) */
    if (atomic_exchange(&s_state_dirty, false)) {
        ui_update(state_get());
    }

    state_tick();

    const status_state_t *state = state_get();
    int32_t seconds = state_timer_get_seconds();
    ui_update_timer(seconds, state->timer_type);

    /* Update clock + WiFi status every 30 seconds */
    if (++s_time_update_counter >= 30) {
        s_time_update_counter = 0;

        char time_str[16], date_str[16];
        ntp_get_time_str(time_str, sizeof(time_str));
        ntp_get_date_str(date_str, sizeof(date_str));
        ui_update_time(time_str, date_str);

        wifi_state_t ws = network_get_state();
        bool connected = (ws == WIFI_STATE_CONNECTED);
        ui_update_wifi_status(network_get_ip(), connected,
                              connected ? network_get_rssi_percent() : -1);

        /* Weather display */
        const weather_data_t *w = weather_get();
        ui_update_weather(w->temp_f, weather_code_icon(w->weather_code),
                          w->precip_chance, w->valid);

        /* Auto-dim based on config quiet hours */
        if (ntp_is_synced()) {
            const device_config_t *cfg = config_get();
            time_t now;
            time(&now);
            struct tm ti;
            localtime_r(&now, &ti);
            int hour = ti.tm_hour;
            bool quiet = (cfg->dim_start_hour > cfg->dim_end_hour)
                ? (hour >= cfg->dim_start_hour || hour < cfg->dim_end_hour)
                : (hour >= cfg->dim_start_hour && hour < cfg->dim_end_hour);
            board_backlight_set(quiet ? cfg->dim_brightness : cfg->brightness);
        }
    }
}

/* ── LVGL task — Core 0, dedicated ── */
static void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        if (time_till_next < 2) time_till_next = 2;
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Tipboard Phase 2A starting...");

    /* ── Hardware init ── */
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_display_init(&panel));
    ESP_ERROR_CHECK(board_backlight_init());

    esp_lcd_touch_handle_t touch = NULL;
    esp_err_t touch_err = board_touch_init(&touch);
    if (touch_err == ESP_OK) {
        s_touch = touch;
        ESP_LOGI(TAG, "Touch initialized");
    } else {
        ESP_LOGW(TAG, "Touch init failed (0x%x), continuing without touch", touch_err);
    }

    /* ── LVGL init ── */
    lv_init();

    lv_display_t *disp = lv_display_create(BOARD_DISP_H_RES, BOARD_DISP_V_RES);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, panel);

    /* Draw buffers must be DMA-capable */
    int buf_lines = (BOARD_DISP_V_RES >= 600) ? 50 : 20;
    size_t buf_size = BOARD_DISP_H_RES * buf_lines * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    assert(buf1 && buf2);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

    /* ── Config + State init ── */
    config_init();  /* Load device config from LittleFS (must be before state_init) */
    ESP_ERROR_CHECK(state_init());
    state_register_change_cb(on_state_change, NULL);

    /* ── UI init ── */
    ESP_ERROR_CHECK(ui_init());
    ui_update(state_get());

    /* ── 1-second LVGL timer ── */
    lv_timer_create(one_second_lv_timer_cb, 1000, NULL);

    /* ── Backlight on ── */
    board_backlight_set(100);

    /* ── LVGL task on Core 0 ── */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 0);

    /* ── Network init (WiFi + NTP) ── */
    ESP_ERROR_CHECK(network_init());

    /* Seed WiFi credentials from .env if provided (idempotent — safe to call every boot) */
    if (TIPBOARD_WIFI_SSID[0]) {
        network_set_credentials(TIPBOARD_WIFI_SSID, TIPBOARD_WIFI_PASS);
    }
    network_wifi_connect();
    ntp_init();

    /* ── HTTP server (starts immediately, serves once WiFi connects) ── */
    webserver_start();

    /* ── MQTT (connects to Mosquitto broker for HA integration) ──
     * MQTT client has built-in auto-reconnect with backoff. */
    tipboard_mqtt_init();

    /* ── Weather polling (fetches every 15 min on Core 1) ── */
    weather_init();

    ESP_LOGI(TAG, "Tipboard Phase 2C running");
}
