#include "board.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"

static const char *TAG = "board";

// ── Backlight ────────────────────────────────────────────────

esp_err_t board_backlight_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,
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

// ── Display ──────────────────────────────────────────────────

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "Initializing MIPI-DSI display (JD9165)...");

    // Power on MIPI DSI PHY (internal LDO channel 3 at 2.5V)
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    ESP_RETURN_ON_ERROR(
        esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy),
        TAG, "LDO MIPI PHY failed"
    );

    // Create MIPI DSI bus
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 550,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_dsi_bus(&bus_config, &dsi_bus),
        TAG, "DSI bus failed"
    );

    // Create DPI panel (the pixel-streaming interface within DSI)
    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = JD9165_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io),
        TAG, "DBI IO failed"
    );

    // JD9165 panel configuration
    // IMPORTANT: The stock macro values may need adjustment for this board.
    // Cross-reference with cheops repo for proven timing values.
    esp_lcd_dpi_panel_config_t dpi_config = JD9165_1024_600_PANEL_60HZ_DPI_CONFIG(
        BOARD_DISP_H_RES, BOARD_DISP_V_RES
    );
    // DMA2D required for proper display refresh (confirmed by cheops reference)
    dpi_config.flags.use_dma2d = true;

    jd9165_vendor_config_t vendor_config = {
        .dsi_bus = dsi_bus,
        .dpi_config = &dpi_config,
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_DISP_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_jd9165(dbi_io, &panel_config, panel_handle),
        TAG, "JD9165 panel creation failed"
    );

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel_handle), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel_handle), TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*panel_handle, true), TAG, "Display on failed");

    ESP_LOGI(TAG, "Display initialized: %dx%d", BOARD_DISP_H_RES, BOARD_DISP_V_RES);
    return ESP_OK;
}

// ── Touch ────────────────────────────────────────────────────

esp_err_t board_touch_init(esp_lcd_touch_handle_t *touch_handle)
{
    ESP_LOGI(TAG, "Initializing GT911 touch controller...");

    // Create I2C bus
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BOARD_TOUCH_I2C_SCL,
        .sda_io_num = BOARD_TOUCH_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&i2c_bus_config, &i2c_bus),
        TAG, "I2C bus failed"
    );

    // GT911 touch config
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config =
        ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.dev_addr = BOARD_TOUCH_I2C_ADDR;

    // Create I2C device for touch
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io),
        TAG, "Touch IO failed"
    );

    esp_lcd_touch_config_t tp_config = {
        .x_max = BOARD_DISP_H_RES,
        .y_max = BOARD_DISP_V_RES,
        .rst_gpio_num = BOARD_TOUCH_RST_GPIO,
        .int_gpio_num = BOARD_TOUCH_INT_GPIO,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_i2c_gt911(tp_io, &tp_config, touch_handle),
        TAG, "GT911 init failed"
    );

    ESP_LOGI(TAG, "Touch initialized");
    return ESP_OK;
}
