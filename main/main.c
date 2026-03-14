#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board.h"

static const char *TAG = "tipboard";

void app_main(void)
{
    ESP_LOGI(TAG, "Tipboard starting...");

    // Display must init before backlight to avoid white flash
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_display_init(&panel));

    ESP_ERROR_CHECK(board_backlight_init());

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));

    ESP_LOGI(TAG, "Board initialized. Display and touch ready.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
