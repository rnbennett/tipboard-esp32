# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build, Flash, and Monitor

ESP-IDF does NOT work from Git Bash/MSYS2. Use PowerShell scripts.

**Build + Flash (full pipeline):**
```bash
cd "C:/Users/rnben/OneDrive/Documents/Development/tipboard"
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```

**Incremental build only (no flash):**
```bash
powershell.exe -Command "& {
  \$env:PATH='C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;' + \$env:PATH
  Set-Location 'C:\Users\rnben\OneDrive\Documents\Development\tipboard'
  & ninja -C build
}" 2>&1 | tail -20
```

**Serial monitor:**
```bash
powershell.exe -Command "& 'C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe' -m serial.tools.miniterm COM8 115200 2>&1" 2>&1
```
Monitor runs as a background task — use TaskOutput to read it.

**Clean rebuild (required when sdkconfig.defaults changes):**
```bash
rm -f "C:/Users/rnben/OneDrive/Documents/Development/tipboard/sdkconfig"
rm -rf "C:/Users/rnben/OneDrive/Documents/Development/tipboard/build"
```
Then run the full build+flash command above.

## Hardware

- **Board:** JC1060P470C_I_W_Y (Guition ESP32-P4, 7" 1024x600 capacitive touch)
- **Chip:** ESP32-P4 revision v1.3 (RISC-V, not Xtensa)
- **Serial port:** COM8 = ESP32-P4. COM4 = unrelated ESP32 on different cable — do not use.
- **Display:** JD9165 MIPI-DSI driver, esp_lcd_jd9165 v2.0.2
- **Touch:** GT911 via I2C at address 0x5D, requires `driver_data` with `esp_lcd_touch_io_gt911_config_t` for RST/INT address selection sequence
- **PSRAM:** 32MB HEX mode (not OCT), LDO channel 2 at 1900mV
- **Flash:** 16MB QIO

## Architecture

ESP-IDF v5.5.3 project with four components:

- **`components/board/`** — Hardware abstraction layer. All pin definitions in `board.h`, all driver init (MIPI-DSI, I2C touch, LEDC backlight) in `board.c`. Display must init before backlight to avoid white flash.
- **`components/state/`** — State management. Mode/timer/priority logic (`state.c`), LittleFS JSON persistence with 2-second debounce (`persist.c`). Timer state uses monotonic timestamps and is cleared on reboot.
- **`components/ui/`** — LVGL UI. Three-zone screen layout (`ui_screen.c`), WDW Monorail color themes (`ui_theme.c`), Spaceship Earth geodesic hex pattern (`ui_geodesic.c`), timer arc widget (`ui_timer_arc.c`), fade transitions (`ui_transitions.c`). Fonts in `ui/fonts/`.
- **`main/`** — Wires state + UI together. LVGL flush/touch callbacks, tick timer, 1-second `lv_timer` for state ticks (runs in LVGL task context for thread safety). FreeRTOS task pinned to Core 0.

### Fonts
- **Prototype** — 1982 Epcot geometric typeface, converted via `lv_font_conv --no-compress` (RLE compression requires `LV_USE_FONT_COMPRESSED` which is not enabled)
- Active sizes: 120px (hero label), 48px (subtitle/timer), 20px (status bars)
- Additional sizes (96, 72, 28) in repo but not in build — add to `components/ui/CMakeLists.txt` SRCS to re-enable
- Generate new sizes: `npx lv_font_conv --bpp 4 --size SIZE --no-compress --font tools/fonts/Prototype.ttf --range 0x20-0x7F --format lvgl --output components/ui/fonts/font_prototype_SIZE.c --lv-include "lvgl.h"`

### Color Themes
Colors sampled from actual WDW Monorail fleet reference. Each mode maps to a monorail:
- Available=Green, Focused=Gold, Meeting=Red, Away=Blue, Custom=Teal, Streaming=Silver, Pomodoro=Red→Green

Managed components (auto-downloaded by ESP-IDF component manager):
- `lvgl/lvgl` v9.5.0
- `espressif/esp_lcd_jd9165` v2.0.2 — JD9165 v2 API uses `JD9165_PANEL_BUS_DSI_2CH_CONFIG()` macro for DSI bus, `JD9165_1024_600_PANEL_60HZ_DPI_CONFIG(pixel_format)` for DPI config, and nested `.mipi_config` in vendor struct
- `espressif/esp_lcd_touch_gt911` v1.2.0
- `joltwallet/littlefs` — LittleFS filesystem for state persistence

## Key sdkconfig Notes

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` — MUST be set before `CONFIG_ESP32P4_REV_MIN_1=y` takes effect (rev <3.0 and >=3.0 are mutually exclusive in ESP-IDF)
- `CONFIG_SPIRAM_MODE_HEX=y` — this board uses HEX PSRAM, not OCT. Using OCT causes illegal instruction crash at boot.
- `CONFIG_ESP_LDO_RESERVE_PSRAM=y` + channel 2 at 1900mV — required for PSRAM power
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` — increased from default 3584 for LVGL object creation headroom
- When changing `sdkconfig.defaults`, you MUST delete the generated `sdkconfig` file and `build/` directory for changes to take effect
- CMake component name for cJSON is `json` (not `cJSON`), for LittleFS is `littlefs` (registered as `joltwallet/littlefs` in idf_component.yml)

## Reference Repos

Cloned in parent directory for hardware reference:
- `tipboard-reference-cheops/` — Community-adapted firmware for this exact board (proven working configs)
- `tipboard-reference-sukesh/` — Schematics, datasheets, pin maps
