#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

// ── JC1060P470C Pin Definitions ──────────────────────────────

// Display (MIPI DSI)
#define BOARD_DISP_RST_GPIO     5
#define BOARD_DISP_BL_GPIO      23

// Touch (GT911 via I2C)
#define BOARD_TOUCH_I2C_SDA     7
#define BOARD_TOUCH_I2C_SCL     8
#define BOARD_TOUCH_INT_GPIO    21
#define BOARD_TOUCH_RST_GPIO    22
#define BOARD_TOUCH_I2C_ADDR    0x5D
#define BOARD_TOUCH_I2C_FREQ    400000

// Display parameters
#define BOARD_DISP_H_RES        1024
#define BOARD_DISP_V_RES        600
#define BOARD_DISP_DPI_CLK_MHZ  51

// User button
#define BOARD_BUTTON_GPIO       35

// ── Board Init API ───────────────────────────────────────────

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel_handle);
esp_err_t board_touch_init(esp_lcd_touch_handle_t *touch_handle);
esp_err_t board_backlight_init(void);
esp_err_t board_backlight_set(uint8_t brightness_pct);
