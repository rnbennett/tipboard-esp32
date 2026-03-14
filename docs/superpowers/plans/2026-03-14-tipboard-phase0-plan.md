# Tipboard Phase 0 — Framework Validation Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Validate that ESP-IDF + LVGL v9 runs on the JC1060P470C board with working display and touch, establishing the foundation for all subsequent phases.

**Architecture:** ESP-IDF v5.3+ project using MIPI-DSI display (JD9165 driver) and GT911 capacitive touch via I2C. The ESP32-P4 has no built-in WiFi/Bluetooth — wireless comes from a companion ESP32-C6 module via SDIO (ESP-Hosted), but that is Phase 2 work. Phase 0 focuses purely on display + touch + LVGL.

**Tech Stack:** ESP-IDF v5.3+ (CMake/idf.py), LVGL v9, esp_lcd_jd9165 managed component, esp_lcd_touch_gt911 managed component, C language.

**Critical context:** Arduino support for ESP32-P4 exists (v3.1.0+) but MIPI-DSI peripheral support is listed as "pending." ESP-IDF is the mature, recommended path. PlatformIO does not officially support ESP32-P4. We use native ESP-IDF tooling (idf.py).

**Key references:**
- [cheops/JC1060P470C_I_W](https://github.com/cheops/JC1060P470C_I_W) — community-adapted firmware that works on this exact board (stock Espressif examples do NOT work without modifications)
- [sukesh-ak/JC1060P470C_I_W-GUITION-ESP32-P4_ESP32-C6](https://github.com/sukesh-ak/JC1060P470C_I_W-GUITION-ESP32-P4_ESP32-C6) — schematics, datasheets, pin maps, demo code
- [esp-dev-kits/examples/esp32-p4-function-ev-board/examples/lvgl_demo_v9](https://github.com/espressif/esp-dev-kits/tree/master/examples/esp32-p4-function-ev-board/examples/lvgl_demo_v9) — official LVGL v9 example for ESP32-P4

---

## Chunk 1: Environment Setup & Project Scaffold

### File Structure

This is the complete project structure we are building toward (Phase 0 creates the foundation; later phases add components):

```
tipboard/
├── CMakeLists.txt                          # Top-level ESP-IDF project
├── sdkconfig.defaults                      # Board-specific sdkconfig
├── partitions.csv                          # Custom partition table
├── main/
│   ├── CMakeLists.txt
│   └── main.c                              # Entry point, FreeRTOS tasks
├── components/
│   ├── board/                              # Board-specific hardware init
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── board.h                     # Pin definitions + init API
│   │   └── board.c                         # Display + touch initialization
│   ├── state/                              # Pure logic: state, modes, timer
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── state_manager.h
│   │   │   ├── mode.h
│   │   │   └── timer.h
│   │   ├── state_manager.c
│   │   ├── mode.c
│   │   └── timer.c
│   ├── config/                             # LittleFS persistence
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── config_manager.h
│   │   └── config_manager.c
│   ├── ui/                                 # LVGL screens and widgets
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── ui_manager.h
│   │   │   ├── status_screen.h
│   │   │   ├── top_bar.h
│   │   │   ├── hero_zone.h
│   │   │   ├── bottom_bar.h
│   │   │   └── geodesic_bg.h
│   │   ├── ui_manager.c
│   │   ├── status_screen.c
│   │   ├── top_bar.c
│   │   ├── hero_zone.c
│   │   ├── bottom_bar.c
│   │   ├── geodesic_bg.c
│   │   └── theme/
│   │       ├── colors.h
│   │       ├── colors.c
│   │       └── fonts.h
│   ├── network/                            # Phase 2+
│   └── input/                              # Phase 4+
├── managed_components/                     # Auto-downloaded by ESP-IDF
├── data/                                   # LittleFS partition (Phase 2+)
├── fonts/
│   ├── Prototype.ttf
│   └── generate_fonts.sh
├── test/                                   # Unity tests for pure logic
│   ├── CMakeLists.txt
│   └── state/
│       ├── test_mode.c
│       ├── test_timer.c
│       └── test_state_manager.c
└── docs/
    └── superpowers/
        ├── specs/
        │   └── 2026-03-14-tipboard-design.md
        └── plans/
            └── 2026-03-14-tipboard-phase0-plan.md
```

### Board Pin Definitions (JC1060P470C)

These are locked — derived from schematics and confirmed by community repos:

| GPIO | Function            | Notes                              |
|------|---------------------|------------------------------------|
| 5    | Display Reset       | Active low                         |
| 23   | Backlight PWM       | LEDC channel                       |
| 7    | I2C SDA             | Shared bus (touch, RTC, audio)     |
| 8    | I2C SCL             | 400 kHz                            |
| 21   | Touch Interrupt     | GT911 INT pin                      |
| 22   | Touch Reset         | GT911 RST pin                      |
| 35   | User Button (BOOT)  | SW1, active low                    |
| MIPI | DSI data/clock      | Direct (no GPIO assignment needed) |

---

### Task 1: Install ESP-IDF and verify ESP32-P4 target

**Files:** None (environment setup)

- [ ] **Step 1: Install ESP-IDF v5.3+ (or latest stable)**

Follow the official guide for your OS:
- Windows: https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/windows-setup.html
- The ESP-IDF installer will set up Python, CMake, Ninja, and the RISC-V toolchain.

```bash
# After installation, verify:
idf.py --version
```

Expected: version 5.3.x or higher.

- [ ] **Step 2: Verify ESP32-P4 target is available**

```bash
idf.py --list-targets
```

Expected: output includes `esp32p4`.

- [ ] **Step 3: Clone reference repositories for this board**

```bash
cd C:\Users\rnben\OneDrive\Documents\Development
git clone https://github.com/cheops/JC1060P470C_I_W.git tipboard-reference-cheops
git clone https://github.com/sukesh-ak/JC1060P470C_I_W-GUITION-ESP32-P4_ESP32-C6.git tipboard-reference-sukesh
```

These contain working code, schematics, and driver configurations we will reference throughout.

---

### Task 2: Create ESP-IDF project scaffold

**Files:**
- Create: `CMakeLists.txt`
- Create: `main/CMakeLists.txt`
- Create: `main/main.c`
- Create: `main/idf_component.yml`
- Create: `sdkconfig.defaults`
- Create: `partitions.csv`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Include component directories
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tipboard)
```

- [ ] **Step 2: Create main/CMakeLists.txt**

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board
)
```

- [ ] **Step 3: Create main/idf_component.yml for managed components**

```yaml
# main/idf_component.yml
dependencies:
  lvgl/lvgl:
    version: "^9"
  espressif/esp_lcd_jd9165:
    version: "*"
  espressif/esp_lcd_touch_gt911:
    version: "*"
```

- [ ] **Step 4: Create minimal main.c**

```c
// main/main.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "tipboard";

void app_main(void)
{
    ESP_LOGI(TAG, "Tipboard starting...");

    while (1) {
        ESP_LOGI(TAG, "Heartbeat");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

- [ ] **Step 5: Create sdkconfig.defaults**

These settings are specific to the JC1060P470C board. Values derived from the cheops reference repo:

```ini
# sdkconfig.defaults

# Target
CONFIG_IDF_TARGET="esp32p4"

# Flash
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y

# PSRAM (octal PSRAM on this board)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y

# LVGL
CONFIG_LV_USE_DEMO_WIDGETS=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_48=y

# Partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# FreeRTOS
CONFIG_FREERTOS_HZ=1000

# Log level
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

**Note:** These defaults are a starting point. The exact sdkconfig values for MIPI-DSI, PSRAM, and display timing may need adjustment based on the reference repos. Check the cheops repo's sdkconfig for proven values.

- [ ] **Step 6: Create partitions.csv**

Custom partition table with OTA support and LittleFS:

```csv
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     ,         0x6000,
otadata,   data, ota,     ,         0x2000,
phy_init,  data, phy,     ,         0x1000,
ota_0,     app,  ota_0,   ,         0x300000,
ota_1,     app,  ota_1,   ,         0x300000,
# "spiffs" subtype but will be used with LittleFS (LittleFS uses the spiffs partition type)
storage,   data, spiffs,  ,         0x200000,
```

This gives 3MB per OTA slot and 2MB for LittleFS — plenty for the dashboard assets. Total: ~8.6MB of 16MB flash used.

- [ ] **Step 7: Build and flash the minimal project**

```bash
cd C:\Users\rnben\OneDrive\Documents\Development\tipboard
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with your board's serial port. To find it: open Device Manager → Ports (COM & LPT), or run `idf.py -p list`.

Expected: Serial monitor shows "Tipboard starting..." and "Heartbeat" every second.

**Note:** The ESP32-P4 uses a RISC-V core (not Xtensa like ESP32/ESP32-S3). The ESP-IDF installer should have set up the RISC-V toolchain automatically, but if you get toolchain errors, verify `riscv32-esp-elf-gcc` is on your PATH.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt main/ sdkconfig.defaults partitions.csv
git commit -m "feat: ESP-IDF project scaffold for ESP32-P4"
```

---

### Task 3: Create board component with pin definitions

**Files:**
- Create: `components/board/CMakeLists.txt`
- Create: `components/board/include/board.h`
- Create: `components/board/board.c`

- [ ] **Step 1: Create board component CMakeLists.txt**

```cmake
# components/board/CMakeLists.txt
idf_component_register(
    SRCS "board.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_lcd_touch esp_hw_support
)
```

- [ ] **Step 2: Create board.h with pin definitions**

```c
// components/board/include/board.h
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

/**
 * Initialize the MIPI-DSI display with JD9165 driver.
 * Returns the panel handle for LVGL integration.
 */
esp_err_t board_display_init(esp_lcd_panel_handle_t *panel_handle);

/**
 * Initialize GT911 touch controller.
 * Returns the touch handle for LVGL integration.
 */
esp_err_t board_touch_init(esp_lcd_touch_handle_t *touch_handle);

/**
 * Initialize backlight PWM. Brightness 0-100.
 */
esp_err_t board_backlight_init(void);
esp_err_t board_backlight_set(uint8_t brightness_pct);
```

- [ ] **Step 3: Create board.c stub**

```c
// components/board/board.c
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
        .lane_bit_rate_mbps = 1000,
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
    // The cheops repo found that DMA2D flag is needed:
    // dpi_config.flags.use_dma2d = true;

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
```

**Important caveat:** The MIPI-DSI initialization code above is based on the Espressif API and cheops reference. The exact JD9165 macro parameters, DMA2D flags, and DSI lane configuration may need tuning. The cheops repo's modifications to the JD9165 driver are critical — compare your `esp_lcd_jd9165` component version against their patches.

- [ ] **Step 4: Update main.c to init board**

```c
// main/main.c
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
```

- [ ] **Step 5: Build, flash, verify display initializes**

```bash
idf.py build
idf.py -p COMx flash monitor
```

Expected: Serial log shows successful init of backlight, JD9165, and GT911. The screen should light up (may be blank or show artifacts — that's fine, LVGL isn't running yet).

If display init fails: compare your code against the cheops repo's working example. Common issues:
- Wrong LDO voltage for MIPI PHY
- Wrong DSI lane bit rate
- JD9165 driver needs patched init sequence (check cheops repo patches)

- [ ] **Step 6: Commit**

```bash
git add components/board/
git commit -m "feat: board component with JD9165 display and GT911 touch init"
```

---

## Chunk 2: LVGL Integration & Hello World

### Task 4: Integrate LVGL v9 with display and touch

**Files:**
- Modify: `main/main.c`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Update main/CMakeLists.txt to include LVGL**

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board lvgl
)
```

- [ ] **Step 2: Create LVGL display and touch driver integration in main.c**

```c
// main/main.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
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
    // Note: calling flush_ready synchronously works but is suboptimal for MIPI-DSI
    // DPI mode where esp_lcd_panel_draw_bitmap uses DMA. If tearing or corruption
    // occurs, switch to a DMA completion callback (on_color_trans_done) to signal
    // flush_ready asynchronously. This synchronous approach is fine for Phase 0.
    lv_display_flush_ready(disp);
}

// LVGL touch read callback
static esp_lcd_touch_handle_t s_touch = NULL;

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x[1], y[1];
    uint8_t count = 0;

    esp_lcd_touch_read_data(s_touch);
    bool touched = esp_lcd_touch_get_coordinates(s_touch, x, y, NULL, &count, 1);

    if (touched && count > 0) {
        data->point.x = x[0];
        data->point.y = y[0];
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
    // Starting with 50 rows (~100KB per buffer). With 32MB PSRAM, can increase to
    // full-frame (1024*600*2 = ~1.2MB) if performance needs it. Try DIRECT render
    // mode with full-frame buffers if PARTIAL mode causes tearing.
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

    // ── Start LVGL task on Core 0 ────────────────────
    // IMPORTANT: After this point, all LVGL API calls from other tasks MUST be
    // protected with lv_lock()/lv_unlock() (LVGL v9 thread safety).
    // In Phase 0, all LVGL objects are created before the task starts, so no
    // locking is needed yet. Phase 1+ will add a proper mutex wrapper.
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Tipboard ready.");
}
```

**Note on LVGL v9 API:** The API above uses LVGL v9 conventions (`lv_display_create`, `lv_indev_create`, etc.). If the managed component pulls a slightly different v9 minor version, function signatures may vary. Check the LVGL v9 migration guide if you get compile errors.

- [ ] **Step 3: Build, flash, verify LVGL renders**

```bash
idf.py build
idf.py -p COMx flash monitor
```

Expected: Screen shows dark background with green "TIPBOARD" text and gray subtitle. Touch the screen — the serial log should show touch coordinates (add a temporary log in the touch callback if needed to verify).

- [ ] **Step 4: Add touch feedback to verify input**

Add a touch callback function above `app_main`:

```c
static lv_obj_t *s_coords_label = NULL;

static void screen_tap_cb(lv_event_t *e)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    lv_label_set_text_fmt(s_coords_label, "Touch: %d, %d", point.x, point.y);
}
```

Then add this after the hello world labels in `app_main` (before starting the LVGL task):

```c
    // Touch coordinate display (temporary, for validation)
    lv_obj_t *coords = lv_label_create(lv_screen_active());
    lv_label_set_text(coords, "Touch: ---, ---");
    lv_obj_set_style_text_color(coords, lv_color_hex(0xffff00), 0);
    lv_obj_align(coords, LV_ALIGN_BOTTOM_MID, 0, -20);

    s_coords_label = coords;
    lv_obj_add_event_cb(lv_screen_active(), screen_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(lv_screen_active(), LV_OBJ_FLAG_CLICKABLE);
```

- [ ] **Step 5: Build, flash, verify touch coordinates display on screen**

```bash
idf.py build
idf.py -p COMx flash monitor
```

Expected: Tapping the screen updates the yellow coordinate text. Coordinates should range from (0,0) to (1023,599). If they are inverted or offset, the GT911 touch mapping needs calibration (swap x/y, invert axes in the touch config).

- [ ] **Step 6: Commit**

```bash
git add main/
git commit -m "feat: LVGL v9 hello world with display and touch working"
```

---

### Task 5: Validate performance baseline

**Files:** None (measurement only)

- [ ] **Step 1: Add FPS counter to LVGL**

In main.c, after LVGL init:

```c
    // Enable LVGL performance monitoring
    // LVGL v9: set LV_USE_PERF_MONITOR in lv_conf.h or via sdkconfig
```

Add to sdkconfig.defaults:

```ini
CONFIG_LV_USE_PERF_MONITOR=y
```

- [ ] **Step 2: Build, flash, check FPS**

Expected: FPS counter appears on screen. Target is ≥30 FPS for smooth animations. The ESP32-P4 at 400 MHz with MIPI-DSI should handle this easily for a 1024x600 display.

If FPS is low:
- Increase draw buffer size (try `BOARD_DISP_H_RES * 100` or full-screen buffer)
- Enable DMA2D in the DPI config
- Check PSRAM speed is set to 200 MHz

- [ ] **Step 3: Document results and commit**

```bash
git add sdkconfig.defaults
git commit -m "feat: add LVGL perf monitor, validate rendering performance"
```

---

## Phase 0 Completion Criteria

Phase 0 is **PASS** when all of the following are true:

1. ESP-IDF builds and flashes successfully targeting esp32p4
2. MIPI-DSI display shows LVGL-rendered content (the hello world screen)
3. GT911 touch input is working (coordinates display correctly on tap)
4. LVGL renders at ≥30 FPS
5. Backlight brightness can be controlled via `board_backlight_set()`

If any of these fail, troubleshoot using:
- The cheops repo's known-working code as a reference
- The sukesh-ak repo's schematics for pin verification
- ESP-IDF MIPI-DSI documentation and examples

**Do not proceed to Phase 1 until Phase 0 passes.**

---

## What Comes Next

Phase 1 (Display & Touch) will be planned in a separate document after Phase 0 is validated, covering:
- State management component (mode, timer, Pomodoro state machine)
- Configuration persistence (LittleFS)
- Prototype font conversion to LVGL bitmap format
- Spaceship Earth geodesic pattern renderer
- Full status screen with three-zone layout (both variants for comparison)
- Touch interaction (mode cycling, swipe, timer tap)
- Slide transitions between modes
- Mode color themes (monorail-inspired palette)

Phases 2-5 (WiFi/Web, MQTT/HA, BLE, Polish) will each get their own plan documents as earlier phases complete, since the ESP-Hosted WiFi stack and async networking on ESP32-P4 may require adjustments based on Phase 0/1 findings.
