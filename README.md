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
   git clone https://github.com/<your-username>/tipboard-esp32.git
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
