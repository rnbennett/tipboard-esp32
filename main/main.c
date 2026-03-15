// main/main.c
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "board.h"

static const char *TAG = "tipboard";

// LVGL display flush callback
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel,
        area->x1, area->y1,
        area->x2 + 1, area->y2 + 1,
        px_map);
    lv_display_flush_ready(disp);
}

// LVGL touch read callback
static esp_lcd_touch_handle_t s_touch = NULL;

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
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

// LVGL tick callback
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(2);
}

// LVGL task — runs on Core 0, dedicated
static void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        if (time_till_next < 2) time_till_next = 2;
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

// Touch coordinate display callback
static lv_obj_t *s_coords_label = NULL;

static void screen_tap_cb(lv_event_t *e)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_label_set_text_fmt(s_coords_label, "Touch: %" PRId32 ", %" PRId32, point.x, point.y);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Tipboard starting...");

    // ── Hardware init ────────────────────────────────
    // Display must init before backlight to avoid white flash
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_display_init(&panel));

    ESP_ERROR_CHECK(board_backlight_init());

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));
    s_touch = touch;

    // ── LVGL init ────────────────────────────────────
    lv_init();

    // Create display
    lv_display_t *disp = lv_display_create(BOARD_DISP_H_RES, BOARD_DISP_V_RES);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, panel);

    // Allocate draw buffers in PSRAM
    size_t buf_size = BOARD_DISP_H_RES * 50 * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create touch input device
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    // Tick timer (2ms)
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000)); // 2ms

    // ── Hello World screen ───────────────────────────
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "TIPBOARD");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *sub = lv_label_create(lv_screen_active());
    lv_label_set_text(sub, "Touch anywhere to test");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_24, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 30);

    // Touch coordinate display (temporary, for validation)
    lv_obj_t *coords = lv_label_create(lv_screen_active());
    lv_label_set_text(coords, "Touch: ---, ---");
    lv_obj_set_style_text_color(coords, lv_color_hex(0xffff00), 0);
    lv_obj_align(coords, LV_ALIGN_BOTTOM_MID, 0, -20);

    s_coords_label = coords;
    lv_obj_add_event_cb(lv_screen_active(), screen_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(lv_screen_active(), LV_OBJ_FLAG_CLICKABLE);

    // ── Start LVGL task on Core 0 ────────────────────
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Tipboard ready.");
}
