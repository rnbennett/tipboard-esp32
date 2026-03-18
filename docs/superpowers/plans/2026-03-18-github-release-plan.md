# GitHub Public Release Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare the tipboard repository for public release on GitHub as `tipboard-esp32`.

**Architecture:** Documentation + cleanup pass — no firmware code changes except fixing stale comments in `state.h`. New files: README.md, LICENSE, CONTRIBUTING.md. Modifications: streamdeck docs, build scripts, .gitignore, .env.example.

**Tech Stack:** Markdown, PowerShell, Git

---

## Task 1: Add LICENSE file

**Files:**
- Create: `LICENSE`

- [ ] **Step 1: Create MIT license file**

```
MIT License

Copyright (c) 2026 Ryan Bennett

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 2: Commit**

```bash
git add LICENSE
git commit -m "chore: add MIT license"
```

---

## Task 2: Fix stale color comments in state.h

**Files:**
- Modify: `components/state/include/state.h:8-17`

- [ ] **Step 1: Update enum comments to match ui_theme.c colors**

Change the three incorrect comments in the `status_mode_t` enum:

```c
/* Seven status modes matching the WDW Monorail color fleet */
typedef enum {
    MODE_AVAILABLE = 0,   /* Green — Monorail Green */
    MODE_FOCUSED,         /* Gold — Monorail Gold */
    MODE_MEETING,         /* Red — Monorail Red */
    MODE_AWAY,            /* Blue — Monorail Blue */
    MODE_POMODORO,        /* Red→Green progression */
    MODE_CUSTOM,          /* Teal — Monorail Teal */
    MODE_STREAMING,       /* Silver — Monorail Silver */
    MODE_COUNT
} status_mode_t;
```

Lines changing:
- Line 10: `Amber/Gold` → `Gold`
- Line 12: `Cool Gray — Monorail Silver` → `Blue — Monorail Blue`
- Line 14: `Coral/Pink — Monorail Coral` → `Teal — Monorail Teal`
- Line 15: `Bright Red/Pink — Monorail Coral (brighter)` → `Silver — Monorail Silver`

- [ ] **Step 2: Verify build still compiles**

```bash
powershell.exe -Command "& { $env:PATH='C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;' + $env:PATH; Set-Location 'C:\Users\rnben\OneDrive\Documents\Development\tipboard'; & ninja -C build }" 2>&1 | tail -5
```

Expected: Build succeeds (comments only — no code change).

- [ ] **Step 3: Commit**

```bash
git add components/state/include/state.h
git commit -m "fix: correct stale monorail color comments in state.h enum"
```

---

## Task 3: Sanitize docs/streamdeck-setup.md

**Files:**
- Modify: `docs/streamdeck-setup.md`

- [ ] **Step 1: Replace all `10.0.0.30` with `<your-tipboard-ip>`**

There are ~20 occurrences across the file. Use find-and-replace on `10.0.0.30` → `<your-tipboard-ip>`.

- [ ] **Step 2: Replace `C:\tipboard-buttons\` path**

Line 14: Change `C:\tipboard-buttons\` to `a folder like `C:\StreamDeck\``

- [ ] **Step 3: Add note at top about finding the IP**

After the `# Tipboard Stream Deck Setup` heading, add:

```markdown
> **Finding your tipboard's IP:** The IP address is shown in the top status bar of the tipboard display. Replace `<your-tipboard-ip>` in all commands below with your device's actual IP.
```

- [ ] **Step 4: Commit**

```bash
git add docs/streamdeck-setup.md
git commit -m "docs: replace hardcoded IP with placeholder in Stream Deck setup"
```

---

## ~~Task 4: SKIP — merged into Task 10~~

---

## Task 5: Update .env.example

**Files:**
- Modify: `.env.example`

- [ ] **Step 1: Add TIPBOARD_PORT and comments**

Update `.env.example` to:

```
# Tipboard Build-Time Defaults
# Copy this file to .env and fill in your values.
# These become the default config values on first boot.
# All can be changed later via the web dashboard.
# .env is gitignored and never committed.

# Board selection: "p4" (Guition ESP32-P4 7") or "cyd" (ESP32-2432S028R)
TIPBOARD_BOARD=p4

# Serial port for flashing (used by PowerShell build scripts)
# Examples: COM8 (Windows), /dev/ttyUSB0 (Linux), /dev/cu.usbmodem* (macOS)
TIPBOARD_PORT=COM8

# POSIX timezone string (see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html)
TIPBOARD_TIMEZONE=EST5EDT,M3.2.0,M11.1.0

# Weather location (latitude/longitude for Open-Meteo API)
TIPBOARD_WEATHER_LAT=40.7128
TIPBOARD_WEATHER_LON=-74.0060

# MQTT broker URI (leave empty to disable MQTT)
TIPBOARD_MQTT_BROKER=mqtt://192.168.1.100:1883

# WiFi credentials (leave empty to start in AP mode for setup)
TIPBOARD_WIFI_SSID=
TIPBOARD_WIFI_PASS=
```

- [ ] **Step 2: Commit**

```bash
git add .env.example
git commit -m "docs: improve .env.example with comments and TIPBOARD_PORT option"
```

---

## Task 6: Parameterize build scripts

**Files:**
- Modify: `build_flash.ps1`
- Modify: `build_flash_cyd.ps1`
- Modify: `build_flash_full.ps1`
- Modify: `build_flash_cyd_full.ps1`

The goal is to read `TIPBOARD_PORT` from `.env` if present, falling back to the current hardcoded values. Add a comment block at the top explaining what to customize. **Do not change any build logic.**

> **Spec deviation:** The spec mentions `TIPBOARD_PATH` from `.env`, but `$PSScriptRoot` (PowerShell's built-in script directory variable) is a better solution — it automatically resolves to wherever the script lives, with zero configuration. No `TIPBOARD_PATH` variable needed.

- [ ] **Step 1: Add .env reader + header to build_flash.ps1**

Add at the very top of `build_flash.ps1`, before the existing content:

```powershell
# ── Tipboard Build + Flash (P4 - ESP32-P4) ──
# Edit the paths below to match your ESP-IDF installation.
# Or set TIPBOARD_PORT in your .env file to override the COM port.

# Read TIPBOARD_PORT from .env if it exists
$TIPBOARD_PORT = "COM8"  # default
$envFile = Join-Path $PSScriptRoot ".env"
if (Test-Path $envFile) {
    Get-Content $envFile | ForEach-Object {
        if ($_ -match '^TIPBOARD_PORT=(.+)$') { $TIPBOARD_PORT = $Matches[1] }
    }
}

```

Then replace the hardcoded `COM8` in the esptool flash command (line 19) with `$TIPBOARD_PORT`.

Also replace the hardcoded `Set-Location` path with:
```powershell
Set-Location $PSScriptRoot
```

And replace all hardcoded `C:\Users\rnben\OneDrive\Documents\Development\tipboard` paths in cmake `-DSDKCONFIG=` with `$PSScriptRoot`:
```powershell
"-DSDKCONFIG=$PSScriptRoot\sdkconfig"
```

- [ ] **Step 2: Same treatment for build_flash_full.ps1**

Same pattern: add .env reader at top, replace `COM8` with `$TIPBOARD_PORT`, replace hardcoded paths with `$PSScriptRoot`.

- [ ] **Step 3: Same treatment for build_flash_cyd.ps1**

Same pattern but default port is `COM10`:
```powershell
$TIPBOARD_PORT = "COM10"  # default for CYD
```

Replace `COM10` in esptool command with `$TIPBOARD_PORT`. Replace hardcoded paths with `$PSScriptRoot`.

- [ ] **Step 4: Same treatment for build_flash_cyd_full.ps1**

Same as step 3 — default `COM10`, replace paths with `$PSScriptRoot`.

- [ ] **Step 5: Test P4 build script**

```bash
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```

Expected: Build succeeds and flashes to COM8 (same behavior as before).

- [ ] **Step 6: Commit all four scripts**

```bash
git add build_flash.ps1 build_flash_cyd.ps1 build_flash_full.ps1 build_flash_cyd_full.ps1
git commit -m "chore: parameterize build scripts — read port from .env, use relative paths"
```

---

## Task 7: Create CONTRIBUTING.md

**Files:**
- Create: `CONTRIBUTING.md`

- [ ] **Step 1: Write CONTRIBUTING.md**

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add CONTRIBUTING.md
git commit -m "docs: add CONTRIBUTING.md with dev setup and code conventions"
```

---

## Task 8: Create README.md

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write README.md**

The full README content — see spec section 2.1 for section order and content. Key details:

```markdown
# tipboard-esp32

A desk status indicator board built on ESP32-P4 and ESP32, with LVGL UI, web dashboard, MQTT/Home Assistant integration, and Stream Deck control.

![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.3-blue.svg)

<!-- TODO: Add photo of the board here -->

## Why I Built This

I work from home and people in my house constantly walk into my office — to grab a drink from the mini-fridge, pet the dog, or just see what I'm up to. Sometimes they end up on camera during meetings. Inspired by the [Busy Bar](https://thebusybar.com/) project, I built an inexpensive alternative using hardware from AliExpress that shows my current status. Mirror displays placed around the house (like the kitchen) let everyone check before heading to my office.

## Features

- **7 status modes** — Available, Focused, Meeting, Away, Pomodoro, Custom, Streaming
- **Pomodoro timer** with animated arc visualization
- **Web dashboard** for configuration and real-time control
- **MQTT** integration with Home Assistant
- **Mirror mode** — read-only displays that follow a primary tipboard over MQTT
- **Stream Deck** control via HTTP API (Mac + Windows)
- **OTA firmware updates** via web dashboard
- **WDW Monorail color themes** — colors sampled from the actual Walt Disney World Monorail fleet
- **Epcot-inspired UI** — Spaceship Earth geodesic hex pattern background
- **WiFi AP fallback** for initial setup

## Supported Hardware

### Primary: Guition ESP32-P4 7" (JC1060P470C-I-W-Y)

| Spec | Detail |
|------|--------|
| Chip | ESP32-P4 (RISC-V) + ESP32-C6 WiFi co-processor |
| Display | 7" 1024x600 MIPI-DSI, capacitive touch (GT911) |
| Memory | 32MB PSRAM, 16MB flash |

**Buy:** [AliExpress](https://www.aliexpress.us/item/3256809836514015.html)

**3D printed desk stand** (in `3d-prints/`, requires M2x8 screws):
- [Stand STL](https://github.com/RealDeco/xiaozhi-esphome/blob/main/3D_Files/Guition%20P4%207.0%20(JC1060P470C)%20Stand.stl)
- [Stand 3MF](https://github.com/RealDeco/xiaozhi-esphome/blob/main/3D_Files/Guition%20P4%207.0%20(JC1060P470C)%20Stand.3mf)

### Mirror: ESP32-2432S028R "Cheap Yellow Display" (CYD)

| Spec | Detail |
|------|--------|
| Chip | ESP32 |
| Display | 2.8" 320x240 SPI ILI9341, resistive touch |
| Variant | Dual USB (USB-C + Micro USB) |

**Buy:** [AliExpress (option 1)](https://www.aliexpress.us/item/3256807158937467.html) · [AliExpress (option 2)](https://www.aliexpress.us/item/3256806284604156.html) · [Amazon](https://www.amazon.com/dp/B0DDPXD415)

**3D printed wall case** (in `3d-prints/`, requires M3x8 screws, designed for velcro Command strip mounting):
- [Slim case on MakerWorld](https://makerworld.com/en/models/2171220-slim-minimal-case-for-esp32-cyd-2-4-2-8#profileId-2354976)

## Quick Start

### Prerequisites

- [ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32p4/get-started/index.html) installed

### Build & Flash

1. Clone this repo:
   ```bash
   git clone https://github.com/<your-username>/tipboard-esp32.git  <!-- TODO: update after repo rename -->
   cd tipboard-esp32
   ```

2. Copy and configure `.env`:
   ```bash
   cp .env.example .env
   # Edit .env with your WiFi credentials, MQTT broker, timezone, etc.
   ```

3. Build and flash:

   **ESP32-P4 (primary board):**
   ```bash
   idf.py set-target esp32p4
   idf.py build
   idf.py flash
   ```

   **ESP32 CYD (mirror board):**
   ```bash
   idf.py set-target esp32
   idf.py build
   idf.py flash
   ```

   **Windows users:** PowerShell convenience scripts are provided — `build_flash.ps1` (P4) and `build_flash_cyd.ps1` (CYD). Edit the ESP-IDF paths at the top to match your installation.

4. On first boot with no WiFi configured, the board creates a `tipboard-XXXX` WiFi access point. Connect to it and open `http://192.168.4.1` to configure.

## Configuration

### Build-Time Options (`.env`)

`.env` is gitignored and never committed. Copy `.env.example` to get started.

| Variable | Description | Example |
|----------|-------------|---------|
| `TIPBOARD_BOARD` | Board type: `p4` or `cyd` | `p4` |
| `TIPBOARD_PORT` | Serial port for flashing (build scripts only) | `COM8` |
| `TIPBOARD_TIMEZONE` | POSIX timezone string | `EST5EDT,M3.2.0,M11.1.0` |
| `TIPBOARD_WEATHER_LAT` | Latitude for weather | `40.7128` |
| `TIPBOARD_WEATHER_LON` | Longitude for weather | `-74.0060` |
| `TIPBOARD_MQTT_BROKER` | MQTT broker URI (empty = disabled) | `mqtt://192.168.1.100:1883` |
| `TIPBOARD_WIFI_SSID` | WiFi network name (empty = AP mode) | `MyNetwork` |
| `TIPBOARD_WIFI_PASS` | WiFi password | |

### Web Dashboard

Once connected to WiFi, access the web dashboard at the device's IP address (shown in the top status bar). From the dashboard you can:

- Change status mode and subtitle
- Start/stop timers
- Adjust brightness and dim schedule
- Configure WiFi, MQTT, timezone, and weather location
- Upload OTA firmware updates
- Enable mirror mode

### WiFi Setup

If no WiFi credentials are configured (or the configured network is unavailable), the board starts in AP mode:
- **SSID:** `tipboard-XXXX` (where XXXX is derived from the device MAC)
- **URL:** `http://192.168.4.1`

Configure your WiFi from the dashboard, then the board reboots and connects to your network.

## API Reference

All endpoints accept and return JSON. The board's IP is shown in the top status bar.

| Method | Endpoint | Description | Body |
|--------|----------|-------------|------|
| `GET` | `/api/status` | Get current status | — |
| `PUT` | `/api/status` | Set mode and/or subtitle | `{"mode": 0, "subtitle": "text"}` |
| `POST` | `/api/timer/start` | Start a timer | `{"type": "pomodoro", "work_min": 25}` |
| `POST` | `/api/timer/stop` | Stop the current timer | — |
| `GET` | `/api/modes` | List available modes | — |
| `GET` | `/api/version` | Get firmware version | — |
| `GET` | `/api/config` | Get device configuration | — |
| `PUT` | `/api/config` | Update device configuration | `{...config fields}` |
| `PUT` | `/api/brightness` | Set brightness | `{"value": 0-100}` |
| `POST` | `/api/ota` | Upload firmware binary | multipart/form-data |
| `POST` | `/api/wifi` | Configure WiFi | `{"ssid": "...", "pass": "..."}` |
| `POST` | `/api/reboot` | Reboot device | — |
| `WS` | `/ws` | Real-time status updates | WebSocket |

**Mode values:** 0=Available, 1=Focused, 2=Meeting, 3=Away, 4=Pomodoro, 5=Custom, 6=Streaming

## Integrations

### MQTT / Home Assistant

The tipboard publishes status updates and subscribes to commands over MQTT. Configure your broker URI in `.env` or via the web dashboard.

See [`docs/ha_automations.yaml`](docs/ha_automations.yaml) for sample Home Assistant automations.

### Stream Deck

Control the tipboard from an Elgato Stream Deck using HTTP API calls.

See [`docs/streamdeck-setup.md`](docs/streamdeck-setup.md) for complete setup instructions (Mac + Windows).

## Architecture

```
components/
├── board/          # Hardware abstraction — pins, display, touch, backlight
├── state/          # State management — modes, timers, persistence (LittleFS)
├── ui/             # LVGL UI — screen layout, themes, fonts, animations
├── webserver/      # HTTP API + web dashboard + WebSocket
├── network/        # WiFi station + AP fallback
└── mqtt_client/    # MQTT publish/subscribe
main/
└── main.c          # Wires everything together, LVGL tick/flush callbacks
```

For detailed hardware notes, pin definitions, and development tips, see [`CLAUDE.md`](CLAUDE.md).

## 3D Printed Enclosures

Pre-made enclosure files are in the `3d-prints/` directory.

**ESP32-P4 desk stand** — Requires M2x8 screws
- `Guition Esp32 P4 JC1060P470 Desk Stand.stl`
- `Guition P4 7.0 (JC1060P470C) Stand.3mf`

**CYD wall case** — Requires M3x8 screws, designed for velcro Command strip wall mounting
- `CYD+CASE+v10.3mf` (print-ready)
- Individual STL parts: `obj_1_CYD CASE v10.stl_2.stl` through `obj_4_CYD CASE v10.stl_4.stl`

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, code conventions, and how to add new boards or status modes.

## License

[MIT](LICENSE) — Ryan Bennett, 2026
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: add README for public release"
```

---

## Task 9: Commit spec and internal docs before gitignoring them

**Files:**
- `docs/superpowers/specs/2026-03-18-github-release-design.md` (already exists)
- `docs/superpowers/plans/2026-03-18-github-release-plan.md` (this file)

This must happen BEFORE Task 4 adds `docs/superpowers/` to `.gitignore`.

- [ ] **Step 1: Commit internal docs to preserve in history**

```bash
git add docs/superpowers/
git commit -m "docs: add internal design spec and implementation plan for release prep"
```

---

## Task 10: Final gitignore cleanup (depends on Task 9)

This is Task 4's actual execution — separated because it depends on Task 9 being done first.

- [ ] **Step 1: Update .gitignore with new entries**

Append to `.gitignore`:

```
# Internal development
_disabled/
docs/superpowers/
sdkconfig.defaults.working

# Alternate/experimental files
main/main_minimal.c

# Debug scripts
tools/serial_read_*.ps1
```

- [ ] **Step 2: Remove newly-gitignored files from index**

```bash
git rm -r --cached docs/superpowers/ 2>/dev/null; true
```

(The other files — `_disabled/`, `main_minimal.c`, `sdkconfig.defaults.working`, `tools/serial_read_*.ps1` — are already untracked per git status, so no `git rm` needed.)

- [ ] **Step 3: Commit**

```bash
git add .gitignore
git commit -m "chore: gitignore internal dev files, debug scripts, and planning docs"
```

---

## Task 11: Git history cleanup (FINAL — do last)

**Prerequisite:** All other tasks complete and committed.

- [ ] **Step 1: Install git-filter-repo if needed**

```bash
pip install git-filter-repo
```

- [ ] **Step 2: Rewrite commit e3bde38 message**

The commit message `"fix: correct calendar entity ID to calendar.rbennett_centegix_com"` contains a personal identifier. Rewrite it to `"fix: correct calendar entity ID"`.

```bash
git filter-repo --message-callback '
    if b"rbennett_centegix_com" in message:
        return message.replace(b"calendar.rbennett_centegix_com", b"calendar entity")
    return message
' --force
```

**Warning:** This rewrites all commit hashes from that commit forward. The remote will need a force-push.

- [ ] **Step 3: Re-add remote and force-push**

```bash
git remote add origin <your-remote-url>
git push --force origin master
```

- [ ] **Step 4: Tag the release**

```bash
git tag -a v1.0.0 -m "Initial public release"
git push origin v1.0.0
```

---

## Execution Order

Tasks 1–3 and 5, 7, 8 are independent and can run in parallel.
Task 6 (build scripts) should be tested on real hardware.
Task 9 must run before Task 10.
Task 11 runs last, after everything else is committed.

```
Parallel: Task 1 (LICENSE)
          Task 2 (state.h comments)
          Task 3 (streamdeck docs)
          Task 5 (.env.example)
          Task 7 (CONTRIBUTING.md)
          Task 8 (README.md)

Sequential: Task 6 (build scripts — needs hardware test)
            Task 9 (commit internal docs — BEFORE gitignore)
            Task 10 (gitignore cleanup — depends on 9)
            Task 11 (git history rewrite — LAST)

Note: Task 4 was merged into Task 10 to avoid execution order confusion.
```
