# Contributing to tipboard-esp32

Thanks for your interest in contributing! Here's how to get started.

## Getting Started

1. Install [ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/get-started/index.html)
2. Clone the repo
3. Copy `.env.example` to `.env` and configure for your setup
4. Build with `idf.py build` (works on Linux, macOS, and Windows)

## Building

The standard ESP-IDF workflow works on all platforms:

```bash
# For ESP32-P4 board
idf.py set-target esp32p4
idf.py build
idf.py flash

# For CYD (ESP32-2432S028R) board
idf.py set-target esp32
idf.py build
idf.py flash
```

Windows users: PowerShell convenience scripts are provided (`build_flash.ps1`, `build_flash_cyd.ps1`). Edit the ESP-IDF paths at the top to match your installation.

## Code Conventions

- **ESP-IDF patterns** — Use `ESP_LOGI`/`ESP_LOGW`/`ESP_LOGE` for logging, `ESP_ERROR_CHECK` for error handling
- **LVGL 9.x API** — UI code uses LVGL v9.5 conventions
- **Multi-board support** — Hardware-specific code is gated by `#ifdef BOARD_P4` / `#ifdef BOARD_CYD`
- **State management** — All state changes go through `components/state/state.c`. UI reads from state, never writes directly.
- **Hardware abstraction** — All pin definitions live in `components/board/board.h`. Driver init lives in `board.c` (P4) and `boards/cyd/board_cyd.c` (CYD).

## Adding a New Board

1. Create `components/board/boards/<name>/board_<name>.c` with display, touch, and backlight init
2. Add `#ifdef BOARD_<NAME>` gate around all hardware-specific code
3. Add the board option to `CMakeLists.txt` (follow the existing `p4`/`cyd` pattern)
4. Add a new `TIPBOARD_BOARD=<name>` option in `.env`
5. Create `sdkconfig.defaults.<name>` with board-specific ESP-IDF settings

## Adding a New Status Mode

1. Add to the `status_mode_t` enum in `components/state/include/state.h`
2. Add a color scheme in `components/ui/ui_theme.c` (`init_schemes()`)
3. Add mode label string in `components/state/state.c` (`state_mode_label()`)
4. Add the mode to the web dashboard UI in `data/` (HTML/JS)

## Submitting Changes

1. **Open an issue first** to discuss what you'd like to change
2. Fork the repo and create a feature branch
3. Test on real hardware before submitting
4. Follow existing code patterns and conventions
5. Keep pull requests focused — one feature or fix per PR
