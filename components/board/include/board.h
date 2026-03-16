#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

/* ── Board Selection ─────────────────────────────────────────
 * Set TIPBOARD_BOARD in .env or CMake:
 *   "p4"  = JC1060P470C (ESP32-P4, 7" 1024x600, MIPI-DSI)
 *   "cyd" = ESP32-2432S028R (ESP32, 2.8" 320x240, SPI ILI9341)
 */

#if defined(BOARD_CYD)

/* ── ESP32-2432S028R "Cheap Yellow Display" ─────────────── */
#define BOARD_DISP_H_RES        320
#define BOARD_DISP_V_RES        240
#define BOARD_DISP_BL_GPIO      21

/* ILI9341 SPI (HSPI) */
#define BOARD_LCD_SPI_HOST      SPI2_HOST
#define BOARD_LCD_MOSI          13
#define BOARD_LCD_MISO          12
#define BOARD_LCD_SCLK          14
#define BOARD_LCD_CS            15
#define BOARD_LCD_DC            2
#define BOARD_LCD_RST           (-1)

/* XPT2046 touch SPI (VSPI) */
#define BOARD_TOUCH_SPI_HOST    SPI3_HOST
#define BOARD_TOUCH_MOSI        32
#define BOARD_TOUCH_MISO        39
#define BOARD_TOUCH_SCLK        25
#define BOARD_TOUCH_CS          33
#define BOARD_TOUCH_IRQ         36

/* RGB LED (active low) */
#define BOARD_LED_R             4
#define BOARD_LED_G             16
#define BOARD_LED_B             17

#elif defined(BOARD_P4)

/* ── JC1060P470C (ESP32-P4) ────────────────────────────── */
#define BOARD_DISP_H_RES        1024
#define BOARD_DISP_V_RES        600
#define BOARD_DISP_RST_GPIO     5
#define BOARD_DISP_BL_GPIO      23
#define BOARD_DISP_DPI_CLK_MHZ  51

/* GT911 touch I2C */
#define BOARD_TOUCH_I2C_SDA     7
#define BOARD_TOUCH_I2C_SCL     8
#define BOARD_TOUCH_INT_GPIO    21
#define BOARD_TOUCH_RST_GPIO    22
#define BOARD_TOUCH_I2C_ADDR    0x5D
#define BOARD_TOUCH_I2C_FREQ    400000

/* User button */
#define BOARD_BUTTON_GPIO       35

#else
#error "No board defined. Set BOARD_P4 or BOARD_CYD via TIPBOARD_BOARD in .env"
#endif

/* ── Board Init API (same for all boards) ────────────────── */

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel_handle);
esp_err_t board_touch_init(esp_lcd_touch_handle_t *touch_handle);
esp_err_t board_backlight_init(void);
esp_err_t board_backlight_set(uint8_t brightness_pct);
