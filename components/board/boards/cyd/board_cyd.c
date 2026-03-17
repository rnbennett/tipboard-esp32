#include "board.h"

#ifdef BOARD_CYD

#include "esp_log.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_xpt2046.h"
/* Note: atanisoft/esp_lcd_touch_xpt2046 component */

static const char *TAG = "board_cyd";

/* ── Backlight ── */

esp_err_t board_backlight_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_conf), TAG, "BL timer failed");

    ledc_channel_config_t ch_conf = {
        .gpio_num = BOARD_DISP_BL_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_conf), TAG, "BL channel failed");

    return board_backlight_set(100);
}

esp_err_t board_backlight_set(uint8_t brightness_pct)
{
    if (brightness_pct > 100) brightness_pct = 100;
    uint32_t duty = (1023 * brightness_pct) / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty), TAG, "BL set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG, "BL update duty failed");
    ESP_LOGI(TAG, "Backlight set to %d%%", brightness_pct);
    return ESP_OK;
}

/* ── Display (ILI9341 via SPI) ── */

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "Initializing ILI9341 display (SPI)...");

    /* SPI bus for display (HSPI) */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = BOARD_LCD_MISO,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 30 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
        TAG, "SPI bus init failed"
    );

    /* LCD panel IO (SPI) — proven working config from limpens/esp32-2432S028R */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC,
        .cs_gpio_num = BOARD_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi(BOARD_LCD_SPI_HOST, &io_config, &io_handle),
        TAG, "LCD IO init failed"
    );

    /* ILI9341 panel — BGR color space for CYD */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_ili9341(io_handle, &panel_config, panel_handle),
        TAG, "ILI9341 panel init failed"
    );

    esp_lcd_panel_reset(*panel_handle);
    esp_lcd_panel_init(*panel_handle);
    esp_lcd_panel_mirror(*panel_handle, false, true);
    esp_lcd_panel_disp_on_off(*panel_handle, true);

    ESP_LOGI(TAG, "Display initialized: %dx%d (native portrait)", 240, 320);
    return ESP_OK;
}

/* ── Touch (XPT2046 via SPI) ── */

esp_err_t board_touch_init(esp_lcd_touch_handle_t *touch_handle)
{
    ESP_LOGI(TAG, "Initializing XPT2046 touch controller (SPI)...");

    spi_bus_config_t touch_bus_cfg = {
        .mosi_io_num = BOARD_TOUCH_MOSI,
        .miso_io_num = BOARD_TOUCH_MISO,
        .sclk_io_num = BOARD_TOUCH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(BOARD_TOUCH_SPI_HOST, &touch_bus_cfg, SPI_DMA_CH_AUTO),
        TAG, "Touch SPI bus init failed"
    );

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(BOARD_TOUCH_CS);
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi(BOARD_TOUCH_SPI_HOST, &tp_io_config, &tp_io),
        TAG, "Touch IO init failed"
    );

    esp_lcd_touch_config_t tp_config = {
        .x_max = 240,
        .y_max = 320,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .mirror_x = true,
            .mirror_y = false,
        },
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_config, touch_handle),
        TAG, "XPT2046 init failed"
    );

    ESP_LOGI(TAG, "Touch initialized");
    return ESP_OK;
}

#endif /* BOARD_CYD */
