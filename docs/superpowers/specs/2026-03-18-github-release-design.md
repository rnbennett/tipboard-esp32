# GitHub Public Release — Design Spec

**Date:** 2026-03-18
**Status:** Approved
**Goal:** Prepare tipboard-esp32 for public GitHub release

---

## 1. Repository Identity

- **Name:** `tipboard-esp32`
- **Description:** A desk status indicator board built on ESP32-P4 and ESP32, with LVGL UI, web dashboard, MQTT/Home Assistant integration, and Stream Deck control
- **License:** MIT
- **Topics:** `esp32`, `esp32-p4`, `lvgl`, `home-assistant`, `mqtt`, `status-indicator`, `esp-idf`

## 2. New Files to Create

### 2.1 README.md

**Sections in order:**

1. **Header** — `# tipboard-esp32` + one-liner description + badges (MIT license, ESP-IDF v5.5.3)
2. **Why I Built This** — 3-4 sentences: work from home, people walk in during meetings, inspired by Busy Bar, built an inexpensive alternative with AliExpress hardware + mirror displays for other rooms
3. **Features** — Bullet list:
   - 7 status modes: Available, Focused, Meeting, Away, Pomodoro, Custom, Streaming
   - Pomodoro timer with animated arc visualization
   - Web dashboard for configuration and real-time control
   - MQTT integration with Home Assistant
   - Mirror mode — read-only displays that follow a primary tipboard over MQTT
   - Stream Deck control via HTTP API (Mac + Windows)
   - OTA firmware updates via web dashboard
   - WDW Monorail color themes (colors sampled from actual fleet)
   - Epcot-inspired Spaceship Earth geodesic hex pattern
   - WiFi AP fallback for initial setup
4. **Supported Hardware** — Two subsections:
   - **Primary: Guition ESP32-P4 7" (JC1060P470C-I-W-Y)** — ESP32-P4 RISC-V, 7" 1024x600 MIPI-DSI capacitive touch, 32MB PSRAM, 16MB flash, ESP32-C6 WiFi co-processor. Link: https://www.aliexpress.us/item/3256809836514015.html. 3D printed desk stand in `3d-prints/` (M2x8 screws). Links to external stand models.
   - **Mirror: ESP32-2432S028R "Cheap Yellow Display" (CYD)** — ESP32, 2.8" 320x240 SPI ILI9341, resistive touch. Dual USB variant (USB-C + Micro USB). Links: AliExpress (2 listings) + Amazon. 3D printed wall case in `3d-prints/` (M3x8 screws), designed for velcro Command strip mounting. Link to external case model.
   - Note: photo placeholders for both boards (user will add later)
5. **Quick Start** — Numbered steps:
   1. Install ESP-IDF v5.5.3 (link to Espressif install guide)
   2. Clone repo
   3. Copy `.env.example` to `.env`, fill in WiFi credentials, MQTT broker, timezone, weather coords, select board (`p4` or `cyd`)
   4. Standard build: `idf.py set-target esp32p4 && idf.py build && idf.py flash` (for P4) or `idf.py set-target esp32 && idf.py build && idf.py flash` (for CYD)
   5. Connect to `tipboard-XXXX` WiFi AP if no WiFi configured, open 192.168.4.1
   6. Windows users: PowerShell convenience scripts provided (`build_flash.ps1`, `build_flash_cyd.ps1`) — edit paths at top of file to match your ESP-IDF installation
6. **Configuration** — Subsections:
   - `.env` build-time options table (TIPBOARD_BOARD, TIPBOARD_TIMEZONE, TIPBOARD_WEATHER_LAT/LON, TIPBOARD_MQTT_BROKER, TIPBOARD_WIFI_SSID/PASS)
   - Web dashboard (runtime config changes, accessible at device IP)
   - WiFi setup (AP mode fallback, configure via dashboard)
7. **API Reference** — Table of HTTP endpoints:
   - `GET /api/status` — current status
   - `PUT /api/status` — set mode/subtitle `{"mode":0,"subtitle":"text"}`
   - `POST /api/timer/start` — start timer `{"type":"pomodoro","work_min":25}`
   - `POST /api/timer/stop` — stop timer
   - `GET /api/modes` — list available modes
   - `GET /api/version` — firmware version
   - `GET /api/config` / `PUT /api/config` — get/set configuration
   - `PUT /api/brightness` — set brightness `{"value":0-100}`
   - `POST /api/ota` — upload firmware binary
   - `POST /api/wifi` — configure WiFi
   - `POST /api/reboot` — reboot device
   - `WebSocket /ws` — real-time status updates
8. **Integrations** — Subsections:
   - MQTT / Home Assistant — brief explanation + link to `docs/ha_automations.yaml`
   - Stream Deck — brief explanation + link to `docs/streamdeck-setup.md`
9. **Architecture** — Brief paragraph explaining component structure (board, state, ui, webserver, main). Link to `CLAUDE.md` for deep hardware details and development notes.
10. **3D Printed Enclosures** — Contents of `3d-prints/` directory, screw requirements (P4: M2x8, CYD: M3x8), links to original designs
11. **Contributing** — Link to `CONTRIBUTING.md`
12. **License** — MIT, link to LICENSE file

### 2.2 LICENSE

Standard MIT license text with:
- Year: 2026
- Author: Ryan Bennett

### 2.3 CONTRIBUTING.md

**Sections:**

1. **Getting Started** — Install ESP-IDF v5.5.3, clone, `.env` setup
2. **Building** — Cross-platform: `idf.py build`. Windows convenience scripts exist but the standard ESP-IDF workflow works everywhere (Linux, macOS, Windows).
3. **Code Conventions:**
   - ESP-IDF patterns (ESP_LOGI, ESP_ERROR_CHECK, etc.)
   - LVGL 9.x API
   - Multi-board code gated by `#ifdef BOARD_P4` / `#ifdef BOARD_CYD`
   - State changes go through `state.c`, UI reads from state
   - All hardware init in `board.c` / `board_cyd.c`
4. **Adding a New Board** — Create `boards/<name>/board_<name>.c`, add `#ifdef BOARD_<NAME>` gate, add option to `.env` TIPBOARD_BOARD, update CMakeLists.txt
5. **Adding a New Status Mode** — Add to `status_mode_t` enum in `state.h`, add color in `ui_theme.c`, add mode name string, add web dashboard button
6. **Submitting Changes** — Open an issue first to discuss. Test on real hardware. Follow existing code patterns. Keep PRs focused.

## 3. Files to Modify

### 3.1 docs/streamdeck-setup.md
- Replace all instances of `10.0.0.30` with `<your-tipboard-ip>`
- Replace `C:\tipboard-buttons\` with a generic path suggestion

### 3.2 build_flash.ps1 and build_flash_cyd.ps1
- Add comments at top explaining what to change for your system
- Read `TIPBOARD_PORT` from `.env` if present, fall back to current COM8/COM10
- Read `TIPBOARD_PATH` from `.env` if present, fall back to current path
- Keep all existing logic unchanged
- **Test:** After modification, verify the scripts still work by running an incremental build+flash on real hardware

### 3.3 build_flash_full.ps1 and build_flash_cyd_full.ps1
- Same treatment as 3.2 — parameterize paths/ports, add comments
- These are currently untracked; commit them alongside the others

### 3.4 .env.example
- Add `TIPBOARD_PORT=COM8` and `TIPBOARD_PATH=` (optional) with comments
- Already has safe placeholder values — no other changes needed

### 3.5 state.h — Fix stale color comments
- Update enum comments to match actual theme colors from `ui_theme.c`:
  - `MODE_AWAY` → "Blue — Monorail Blue" (not "Cool Gray — Monorail Silver")
  - `MODE_CUSTOM` → "Teal — Monorail Teal" (not "Coral/Pink — Monorail Coral")
  - `MODE_STREAMING` → "Silver — Monorail Silver" (not "Bright Red/Pink")

## 4. Files to Remove from Tracking

- `_disabled/` — old code, add to `.gitignore`
- `docs/superpowers/` — internal planning docs, add to `.gitignore` (commit spec first so it's in history)
- `main/main_minimal.c` — alternate version, add to `.gitignore`
- `sdkconfig.defaults.working` — local override, add to `.gitignore`
- `tools/serial_read_cyd_long.ps1`, `tools/serial_read_full.ps1` — debug scripts, add to `.gitignore`

## 5. .gitignore Updates

Add:
```
# Internal development
_disabled/
docs/superpowers/
sdkconfig.defaults.working

# Alternate/experimental files
main/main_minimal.c
```

## 6. Git History Cleanup (Final Step)

Use `git filter-repo` to sanitize commit `e3bde38` message — remove `rbennett_centegix_com` reference. This rewrites hashes from that commit forward. Do this as the very last step before making the repo public.

**Operational notes:**
- `git filter-repo` requires a fresh clone (or `--force` on an existing repo with remote)
- All commit hashes from `e3bde38` forward will change
- After rewriting, force-push to remote
- Tag the release as `v1.0.0` after all cleanup is complete

**Note:** Author email `ryan@adnovagroup.com` stays as-is per user preference.

## 7. Out of Scope

- Cross-platform build testing (Mac/Linux) — ESP-IDF's `idf.py` is already cross-platform, just needs documentation
- GitHub Actions CI — no CI needed for an ESP-IDF firmware project at this stage
- Board photos — user will add later from iPhone photos
- Repo rename on GitHub — user handles this in GitHub settings
