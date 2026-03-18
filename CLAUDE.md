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
- **Co-processor:** ESP32-C6 for WiFi/BLE, connected to P4 via SDIO. No USB port — can only be flashed via SDIO OTA from the P4.
- **Serial port:** COM8 = ESP32-P4 (USB-Serial-JTAG, VID 303A). COM4 = unrelated ESP32 on different cable — do not use.
- **Display:** JD9165 MIPI-DSI driver, esp_lcd_jd9165 v2.0.2, RGB565, reset GPIO 5. **CRITICAL:** The JD9165 requires a full vendor-specific init sequence (50+ register commands in `board.c` `jd9165_init_cmds[]`). The component's default 2-command init (Sleep Out + Display On) is NOT enough — the panel will show black.
- **Touch:** GT911 via I2C at address 0x5D, requires `driver_data` with `esp_lcd_touch_io_gt911_config_t` for RST/INT address selection sequence
- **PSRAM:** 32MB HEX mode (not OCT), LDO channel 2 at 1900mV
- **Flash:** 16MB QIO
- **Power:** Board needs >600mA — use a quality USB cable on a motherboard port, not a hub. Insufficient power causes USB connect/disconnect loops.

### MIPI-DSI Display Troubleshooting

The MIPI DSI FPC cable is extremely sensitive. If the display stops working:

1. **Check USB cable/port** — must provide >600mA. Constant Windows USB chimes = power issue.
2. **Reseat the FPC cable** — open the white FPC connector latch on the PCB, pull the orange flex cable out, push it back in firmly, close latch. This is the #1 cause of "display init succeeds but screen is blank."
3. **Verify `BOARD_P4` is defined** — check `compile_commands.json` for `-DBOARD_P4` in the compile flags. Without it, `board.c` compiles to an empty file (wrapped in `#ifdef BOARD_P4`).
4. **Diagnostic signals:**
   - "White screen then black" = DSI link works, panel initialized, showing framebuffer (all zeros = black)
   - "Completely blank/dark" = DSI signal not reaching panel (cable issue)
   - Backlight blinks but no image = DSI data lanes broken, backlight pins (29-30 on FPC) are separate

### JD9165 Vendor Init Sequence (CRITICAL)

The esp_lcd_jd9165 component's default init only sends Sleep Out + Display On. This is **dangerously insufficient** — the JD9165 panel needs ~50 vendor-specific register writes (power, VCOM, gamma, timing). Without them, the screen is black even though all DSI/DPI/DMA2D calls return ESP_OK and logs show success.

The proper init is in `board.c` as `jd9165_init_cmds[]`, sourced from the Guition Arduino reference. It is passed via `vendor_config.init_cmds` in `board_display_init()`. **Never remove this.**

### Factory Firmware Note

The factory firmware in `tipboard-reference-cheops/8-Burn operation/` uses the **EK79007** display driver, NOT JD9165. This is a generic Espressif P4-EV demo, not specific to our board's panel. Do not use it as a display test — it will show nothing on our JD9165 panel.

## Multi-Board Support

The project supports two boards via `TIPBOARD_BOARD` in `.env`:
- `p4` (default) = JC1060P470C (ESP32-P4, 7" 1024x600, MIPI-DSI JD9165)
- `cyd` = ESP32-2432S028R (ESP32, 2.8" 320x240, SPI ILI9341)

**How it works:** CMakeLists.txt reads `TIPBOARD_BOARD` from `.env` and sets `-DBOARD_P4` or `-DBOARD_CYD` as a compile definition. `board.h` uses `#if defined(BOARD_CYD) / #elif defined(BOARD_P4)` to select pin definitions. `board.c` and `boards/cyd/board_cyd.c` are both compiled but gated by `#ifdef BOARD_P4` / `#ifdef BOARD_CYD`.

**CRITICAL:** If `BOARD_P4` is not defined, `board.c` compiles to an empty file and the linker silently succeeds but no display/touch/backlight code exists. Always verify `-DBOARD_P4` appears in `build/compile_commands.json`.

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

### C6 Co-processor (WiFi/BLE)

- Connected to P4 via SDIO: CLK=18, CMD=19, D0-D3=14-17, Reset=GPIO 54
- No USB port — cannot be flashed with esptool directly
- Flash via SDIO OTA: use `esp_hosted` example `host_performs_slave_ota` (partition method)
- C6 runs esp_hosted slave firmware (currently v2.3.2)
- Factory C6 binary (`JC1060P470_C6.bin` in cheops repo) is NOT a valid OTA image — it's a stub. Must build proper slave firmware from `esp_hosted:slave` example if upgrading.

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
  - `8-Burn operation/Burn files/` — Factory firmware binaries (P4 uses EK79007 driver — wrong for our JD9165 panel)
  - `5-Schematic/` — Board schematics (PNG): power, LCD, ESP32-P4, connectors, ESP32-C6, USB, codec
  - `1-Demo/Demo_IDF/examples/lvgl_demo_v9/` — LVGL v9 demo with BSP (uses `esp_lvgl_port`)
  - BSP pin definitions: LCD_RST=GPIO 27, LCD_BL=GPIO 23 (our firmware uses GPIO 5 for reset and it works)
- `tipboard-reference-sukesh/` — Schematics, datasheets, pin maps

## Lessons Learned (2026-03-17)

1. **Always verify compile defines in the build output.** The `BOARD_P4` define was missing for days because CMakeLists.txt didn't convert `TIPBOARD_BOARD` from `.env` into a `-D` flag. Check `build/compile_commands.json`.
2. **MIPI DSI FPC cables are fragile.** A loose cable makes all DSI API calls succeed but produces zero visible output. The backlight (separate FPC pins) still works, making it look like a software issue.
3. **USB power matters.** The board needs >600mA. Bad cables or weak USB ports cause connect/disconnect loops that look like crash loops.
4. **The factory firmware uses the wrong display driver (EK79007).** Don't use it as a display test for our JD9165 panel.
5. **The C6 can only be flashed over SDIO from the P4.** There is no USB port for the C6. Use the `host_performs_slave_ota` example from esp_hosted.
6. **The JD9165 needs a full vendor init sequence.** The esp_lcd_jd9165 component only sends Sleep Out + Display On by default. The panel shows black without ~50 vendor register writes (power, VCOM, gamma). The proper init is in `board.c` `jd9165_init_cmds[]`. Previously the display worked because factory firmware had programmed those registers and they persisted across soft resets — until clean rebuilds with hard resets wiped them.
7. **Never do clean rebuilds casually.** Deleting `build/` and `managed_components/` for a simple code change caused hours of display debugging. Use incremental builds (`ninja -C build`) for code changes. Only clean rebuild when `sdkconfig.defaults` actually changes.
