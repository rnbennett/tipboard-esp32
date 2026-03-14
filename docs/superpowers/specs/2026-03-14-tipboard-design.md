# Tipboard — Design Spec

A desk-mounted status display built on the JC1060P470C (ESP32-P4, 7" 1024x600 capacitive touchscreen, 16MB flash, 32MB PSRAM). Inspired by Walt Disney World tip boards and the Busy Bar (busy.bar). Shows current availability status with animated visuals, controlled via touchscreen, BLE macro keypad, web dashboard, REST API, and MQTT.

## Status Modes

Seven modes, each with a distinct color scheme inspired by the WDW Monorail fleet:

| Mode                  | Color Palette   | Monorail Inspiration | Gradient Style                              | Pattern Visibility |
|-----------------------|-----------------|----------------------|---------------------------------------------|--------------------|
| Available             | Green           | Monorail Green       | Gentle pulse                                | High — welcoming   |
| Focused / Deep Work   | Amber/Gold      | Monorail Gold        | Slow wave                                   | Subtle, tone-on-tone — calm |
| In a Meeting          | Red             | Monorail Red         | Steady gradient                             | Medium             |
| Away / BRB            | Cool Gray       | Monorail Silver      | Subtle drift                                | Very quiet         |
| Pomodoro              | Red → Green     | Red shifting to Green| Progress arc shifts color as time runs down | Increases with progress |
| Custom Message        | Coral/Pink      | Monorail Coral       | Configurable                                | Medium             |
| Streaming / On Air    | Bright Red/Pink | Monorail Coral (brighter) | Animated glow                          | High — attention-grabbing |

The default state is configurable per user preference (e.g., "Focused" instead of "Available"). On boot or after any timed status expires, the device reverts to this default.

### Custom Message Mode

Uses the `subtitle` field for the user's free-text message (set via dashboard, API, or MQTT). The coral/pink color scheme is the default but is not user-configurable in MVP — custom color selection is a future enhancement.

### Pomodoro State Machine

Single work+break cycle:

1. **Work period** — countdown timer, red color scheme, geodesic pattern at low visibility.
2. Work timer hits zero → **celebratory burst animation** (stretch goal), automatic transition to break.
3. **Break period** — countdown timer, green color scheme, pattern visibility increases.
4. Break timer hits zero → revert to configured default mode. Priority resets.

- Pomodoro cannot be paused. It can be cancelled (which reverts to default immediately).
- No multi-cycle or long-break support in MVP. Single work+break cycle only.
- Work and break durations are configurable (default: 25 min work, 5 min break).

## Display Design

### Visual Language

Primary inspiration: Epcot Future World digital tip boards and WDW Monorail interior design.

- **Background:** Deep cosmic blue-purple with subtle animated gradient movement (slowly shifting, not static).
- **Geometric pattern:** Spaceship Earth geodesic triangle pattern rendered at low opacity (10-15%) over the background, tinted in the current mode's accent color. Each mode controls pattern visibility (see mode table above) — Gold/Focused is barely-there tone-on-tone for calm, Green/Available is high-contrast and welcoming. Inspired by the monorail interior repeating pattern treatment.
- **Status header bar:** Full-width glowing color bar for the current mode (green, amber, red, etc.), directly inspired by the Future World tip board section headers.
- **Typography:** Prototype font (World Bold recreation, classic 1982 Epcot geometric style) for the status name — all caps, wide letter-spacing. Montserrat (ships with LVGL) for secondary text.
- **Color theming per mode:** The entire screen's color identity shifts when changing modes, just as each WDW monorail has its own color personality with the same design language. The geodesic pattern tint, gradient palette, accent stripe, and header bar all change together.
- **Accent details:** Thin color accent lines at screen edges. Thin horizontal divider lines separating info zones (tip board row style).

### Screen Layout (1024x600, landscape)

Three-zone layout: top info bar, hero status section (~70% of screen), bottom event bar.

```
+--------------------------------------------------+
|  12:45 PM . Thu, Mar 14          72*F (sun) 30%  |  <- Top bar: time/date, weather
+--------------------------------------------------+
| ============ FOCUSED ============================ |  <- Glowing amber header bar
|                                                  |     (Prototype font, wide-spaced caps)
|              Deep Work Session                   |  <- Subtitle (Montserrat)
|                                                  |
|         (o) --------------- 1:23:45              |  <- Timer arc + elapsed/countdown
|                                                  |
|    (cosmic bg + geodesic pattern + gradient)     |
+--------------------------------------------------+
|  (cal) Sprint Planning ................. 2:00 PM |  <- Bottom bar: next calendar event
+--------------------------------------------------+
```

Both this layout and an alternate (secondary info consolidated differently) will be built as LVGL screens early in development to compare on actual hardware.

### Transitions

- **Default:** Slide transitions (new status slides in from right, old slides out left).
- **Special cases (if easy to implement):** Pomodoro completion gets a brief celebratory burst. DND/Focused activation gets a quick pulse/throb before settling. These are stretch goals — slides everywhere works fine as the baseline.

### Touch Interaction

- Tap hero zone to cycle modes, or swipe left/right.
- Long-press for settings overlay.
- Tap bottom bar timer to start/stop.

### Screen Dimming

- Auto-dim based on time of day (configurable quiet hours, e.g., 10 PM–7 AM).
- Brightness controllable via REST API and MQTT for HA automation (e.g., dim when room lights are off).
- Optional screensaver: low-brightness clock display or blank screen after extended idle.

## Data Model

### Status State

Stored in ESP32 memory, persisted to LittleFS on meaningful state changes (mode change, timer start/stop, subtitle change — not transient updates like weather or calendar refreshes):

```json
{
  "mode": "focused",
  "label": "FOCUSED",
  "subtitle": "Deep Work Session",
  "color_scheme": "amber",
  "timer": {
    "type": "elapsed",
    "started_at": 1710423600,
    "duration": null
  },
  "auto_expire": {
    "enabled": true,
    "revert_to": "focused",
    "expires_at": 1710430800
  },
  "source": "manual",
  "priority": 1
}
```

- `timer.type`: "elapsed", "countdown", or "none". Pomodoro uses countdown with configured duration. Focused can use elapsed to show how long you've been working.
- `auto_expire.revert_to`: references the user's configured default mode.
- `source`: tracks who set the status — "manual" (touch), "keypad", "api", "mqtt".
- `priority`: 1 for manual/keypad (highest), 2 for api/mqtt. Manual always wins.

### Priority Rules

- Priority 1 (manual, keypad) always wins. Automation cannot override a manually-set status.
- Priority 2 (API, MQTT) can be overridden by anything.
- When a status auto-expires, priority resets, allowing automation to take over again.
- A new manual action always takes effect regardless of current priority.

### Device Configuration

Persisted in LittleFS:

```json
{
  "default_mode": "focused",
  "wifi_ssid": "...",
  "wifi_password": "...",
  "mqtt_broker": "192.168.x.x",
  "mqtt_port": 1883,
  "pomodoro_work_min": 25,
  "pomodoro_break_min": 5,
  "weather_location": "...",
  "weather_api_key": "...",
  "brightness": 100,
  "dim_start_hour": 22,
  "dim_end_hour": 7,
  "dim_brightness": 20,
  "ble_keypad_mappings": {
    "key1": "toggle_timer",
    "key2": "next_mode",
    "key3": "prev_mode"
  }
}
```

### State Change Flow

1. Input arrives (touch, keypad, API, MQTT).
2. Priority check: reject if current state has higher priority than incoming source.
3. Update state in memory.
4. Persist to LittleFS (meaningful state changes only — debounced, max once per 2 seconds).
5. Trigger display transition animation.
6. Publish new state to MQTT (`tipboard/status`).
7. Notify connected WebSocket clients (for live dashboard updates).

## Communication Architecture

### REST API

Served by ESPAsyncWebServer. No authentication — intentionally unsecured for LAN-only use.

| Endpoint                | Method | Purpose                                    |
|-------------------------|--------|--------------------------------------------|
| `GET /api/status`       | GET    | Current status + full state                |
| `PUT /api/status`       | PUT    | Set mode, subtitle, timer, auto-expire     |
| `POST /api/timer/start` | POST  | Start timer (elapsed or pomodoro)          |
| `POST /api/timer/stop`  | POST  | Stop/reset timer                           |
| `GET /api/modes`        | GET   | List available modes + color schemes       |
| `GET /api/config`       | GET   | Get device configuration                   |
| `PUT /api/config`       | PUT   | Update configuration                       |
| `GET /api/version`      | GET   | Firmware version + API version             |
| `GET /`                 | GET   | Serve web dashboard from LittleFS          |
| `WS /ws`                | WS    | Live state updates pushed to dashboard     |

#### PUT /api/status — Request Body

All fields optional. Only provided fields are updated:

```json
{
  "mode": "meeting",
  "subtitle": "Sprint Planning",
  "timer": {
    "type": "countdown",
    "duration": 2700
  },
  "auto_expire": {
    "enabled": true,
    "duration_min": 45
  }
}
```

Response: the full status state object.

#### POST /api/timer/start — Request Body

```json
{
  "type": "pomodoro"
}
```

Or:

```json
{
  "type": "elapsed"
}
```

#### MQTT Command Payload

The `tipboard/command` topic accepts the same schema as `PUT /api/status`:

```json
{
  "mode": "meeting",
  "subtitle": "Sprint Planning",
  "auto_expire": {
    "enabled": true,
    "duration_min": 45
  }
}
```

Additional command actions:

```json
{ "command": "timer_start", "type": "pomodoro" }
{ "command": "timer_stop" }
{ "command": "brightness", "value": 50 }
```

### MQTT

Connected to existing Mosquitto broker. No TLS — LAN-only, matching the REST API security posture.

| Topic                    | Direction | Purpose                                  |
|--------------------------|-----------|------------------------------------------|
| `tipboard/status`        | Publish   | Current state (JSON) on every change     |
| `tipboard/command`       | Subscribe | Incoming commands (see schema above)     |
| `tipboard/calendar`      | Subscribe | Next calendar event from HA              |
| `tipboard/available`     | Publish   | Online/offline via LWT for HA            |

**Home Assistant auto-discovery:** On boot, publish discovery messages to `homeassistant/sensor/tipboard/config` and `homeassistant/select/tipboard_mode/config` so HA automatically creates entities with no manual YAML. These are standard HA MQTT discovery topics, separate from the device's own operational topics above.

### BLE HID (NimBLE)

- ESP32 acts as BLE HID host, pairs with a programmable macro keypad.
- Keypad sends standard HID key codes.
- Firmware maps key codes to actions via configurable mapping table.
- One-time pairing done through web dashboard settings page.

### WebSocket

The dashboard opens a persistent WebSocket connection and receives state pushes in real-time. Status changes from any source (touch, keypad, MQTT) instantly reflect on any open dashboard.

## External Data Sources

| Data          | Source                     | Method                  | Frequency        |
|---------------|----------------------------|-------------------------|------------------|
| Time/Date/TZ  | NTP servers                | Direct from ESP32       | Every few hours  |
| Weather        | Open-Meteo or OpenWeatherMap | Direct HTTP from ESP32 | Every 15-30 min  |
| Calendar       | Google Calendar via HA     | MQTT subscription       | On change        |
| Meeting auto-switch | HA automation         | MQTT command            | On event start/end |

Principle: ESP32 fetches what it can simply (NTP, weather). Everything requiring complex auth or smart logic routes through HA via MQTT.

### Weather Display

Minimal: current temperature + condition icon + precipitation chance in the top bar. Full forecasts are left to the web dashboard or other apps.

### Calendar Integration

HA publishes next event to `tipboard/calendar` via MQTT:

```json
{
  "title": "Sprint Planning",
  "start": "2026-03-14T14:00:00",
  "end": "2026-03-14T15:00:00"
}
```

The same HA automation can send a mode-change command when an event starts (auto-switch to "In a Meeting"), which the priority system will honor unless overridden by a manual status.

## Web Dashboard

Single-page app served from LittleFS. Vanilla HTML/CSS/JS, no framework. Target size: <100KB total.

### Layout

```
+--------------------------------------------------+
|  TIPBOARD                          * Connected   |
+--------------------------------------------------+
|                                                  |
|  Current Status                                  |
|  +----------------------------------------------+|
|  |  (amber) FOCUSED . 1:23:45 elapsed           ||
|  |  Deep Work Session                            ||
|  +----------------------------------------------+|
|                                                  |
|  Quick Switch                                    |
|  [Available] [Focused] [Meeting] [Away]          |
|  [Pomodoro ] [Stream ] [Custom ]                 |
|                                                  |
|  Timer                                           |
|  [> Start] [Stop]  Duration: [25] min            |
|                                                  |
|  Subtitle                                        |
|  [Deep Work Session_______________] [Set]        |
|                                                  |
|  Auto-Expire                                     |
|  [x Enabled]  Revert after: [60] min             |
|                                                  |
+--------------------------------------------------+
|  (gear) Settings (collapsible)                   |
|  Default mode: [Focused v]                       |
|  Brightness: [====o=====] 80%                    |
|  BLE Keypad: Paired (check)                      |
|  WiFi / MQTT / Weather config...                 |
+--------------------------------------------------+
```

### Behaviors

- Mode buttons are color-coded to match the display's color schemes.
- Status card updates in real-time via WebSocket.
- Settings section is collapsible — not needed day-to-day.
- Mobile-responsive — works on phone screens.
- Connected/disconnected indicator in header.

## Tech Stack

| Component        | Technology                                |
|------------------|-------------------------------------------|
| Framework        | Arduino or ESP-IDF (see validation note)  |
| Display          | LVGL                                      |
| Web server       | ESPAsyncWebServer                         |
| MQTT client      | AsyncMqttClient (async-compatible)        |
| BLE              | NimBLE                                    |
| Filesystem       | LittleFS                                  |
| Status font      | Prototype (World Bold recreation)         |
| Secondary font   | Montserrat (LVGL built-in)                |
| Dashboard        | Vanilla HTML/CSS/JS                       |
| Weather API      | Open-Meteo or OpenWeatherMap              |

**Framework validation (Phase 0):** The ESP32-P4 is a newer chip. Arduino framework support (`arduino-esp32`) must be verified before committing. If Arduino does not yet support the ESP32-P4 target, ESP-IDF is the fallback. LVGL, ESPAsyncWebServer, and NimBLE all work on ESP-IDF directly. The spec is framework-agnostic at the library level — the same libraries and architecture apply either way.

## Task Architecture (FreeRTOS)

The ESP32-P4 is dual-core. Running LVGL, WiFi, MQTT, BLE, and HTTP concurrently requires deliberate task management:

| Task              | Core | Priority | Notes                                      |
|-------------------|------|----------|--------------------------------------------|
| LVGL rendering    | 0    | High     | Dedicated core, mutex-protected. Drives display at ~30 FPS. |
| WiFi + HTTP/WS    | 1    | Medium   | ESPAsyncWebServer runs on the network core. |
| MQTT client       | 1    | Medium   | AsyncMqttClient shares the network core.   |
| Weather polling   | 1    | Low      | Periodic HTTP fetch, runs infrequently.    |
| BLE HID host      | 1    | Medium   | NimBLE manages its own task internally.    |
| State manager     | 1    | High     | Processes input from all sources, enforces priority, dispatches updates. Protected by mutex. |

LVGL is single-threaded and must not be accessed from multiple tasks. All display updates go through the state manager, which queues LVGL operations for the rendering task.

## WiFi Provisioning

On first boot (or when WiFi connection fails after 30 seconds of retries):

1. ESP32 starts in AP mode with a captive portal (SSID: `Tipboard-Setup`).
2. User connects from phone/laptop, enters WiFi credentials and MQTT broker address.
3. Credentials saved to LittleFS, device reboots and connects.
4. AP mode can be re-triggered by long-pressing the screen for 10 seconds (factory reset gesture).

## Failure Modes

| Scenario | Behavior |
|----------|----------|
| WiFi unavailable | Display and touch continue working locally. No weather, no web dashboard, no MQTT. Reconnects with exponential backoff. |
| MQTT broker unreachable | Device functions normally minus HA integration. Reconnects with exponential backoff. State changes are not queued — if MQTT is down when a change happens, that update is lost (HA gets the current state on reconnect). |
| Weather API error/timeout | Last known weather displayed with a stale indicator. Retries on next polling cycle. |
| NTP sync fails | Device uses last known time. If no time has ever been synced (first boot without internet), time display shows "--:--". |
| LittleFS write fails | State remains in memory. Log error. Device continues operating — persistence is best-effort. |
| WebSocket client disconnects | Client reconnects automatically. On reconnect, receives current state immediately. |
| OTA update interrupted | ESP32 OTA uses dual-partition scheme. Failed update rolls back to previous firmware automatically. |
| BLE keypad disconnects | Device continues working via touch and web. BLE scans periodically for reconnection. |

## OTA Updates

Dual-partition OTA scheme (standard ESP32 approach):

- New firmware uploaded via the web dashboard settings page (file upload form).
- Firmware is written to the inactive partition. On success, device reboots into the new partition.
- If the new firmware fails to boot, automatic rollback to the previous partition.
- Dashboard assets (HTML/CSS/JS) are updated separately by uploading to LittleFS via the same settings page, without reflashing firmware.

## Implementation Phases

### Phase 0 — Framework Validation

Before writing any application code:

- Verify ESP32-P4 support in `arduino-esp32`. If unsupported, confirm ESP-IDF setup with LVGL.
- Get a "hello world" LVGL screen rendering on the JC1060P470C display.
- Confirm touch input works.
- This phase is pass/fail — it determines the framework for everything that follows.

### Phase 1 — Display & Touch

Get something running on the hardware.

- LVGL running on the JC1060P470C with 7" display.
- Static status screen with both layout variants to compare on real hardware.
- Prototype font converted to LVGL bitmap format.
- Cosmic background + Spaceship Earth geodesic pattern rendering.
- Touch to cycle modes with slide transitions.
- Timer (elapsed + countdown) working on-screen.
- Pomodoro state machine (work → break → revert to default).
- State persisted to LittleFS across reboots.

### Phase 2 — WiFi & Web Control

Control it from a browser.

- WiFi connection with AP-mode captive portal for provisioning.
- NTP time sync.
- REST API endpoints (full CRUD for status).
- Web dashboard served from LittleFS.
- WebSocket for live dashboard updates.
- Weather API integration.
- OTA update support (firmware + dashboard assets).
- Screen brightness control via dashboard.

### Phase 3 — MQTT & Home Assistant

Smart home integration.

- AsyncMqttClient connecting to Mosquitto broker.
- Publish state changes, subscribe to commands and calendar.
- HA auto-discovery (sensor + select entities appear automatically).
- Calendar events via MQTT from HA.
- Auto-switch to meeting mode via HA automation.
- Priority system enforced (manual beats automation).
- Brightness control via MQTT.

### Phase 4 — BLE Keypad

Physical control.

- NimBLE HID host scanning and pairing.
- Key code to action mapping.
- Configurable mappings via web dashboard settings.
- Keypad pairing flow in settings UI.

### Phase 5 — Polish & Flourishes

Refinement.

- Transition animations refined (slide timing, easing curves).
- Special-case animations (Pomodoro complete, DND pulse).
- Color scheme fine-tuning on real hardware.
- Geodesic pattern opacity and tinting per mode.
- Dashboard mobile responsiveness cleanup.
- Auto-dim based on time of day.
- Progressive downgrade testing on lesser boards (if desired).

## Design Decisions & Rationale

- **Monolith on ESP32:** Everything runs on one device. With 16MB flash, there's ample room for firmware + dashboard assets. OTA updates allow iterating on the dashboard without physical reflashing.
- **LVGL + Arduino (or ESP-IDF):** Best path to smooth, animated, good-looking embedded UI. Arduino preferred for ecosystem; ESP-IDF as fallback if ESP32-P4 Arduino support is immature.
- **AsyncMqttClient over PubSubClient:** PubSubClient is synchronous and blocking, incompatible with ESPAsyncWebServer's async architecture. AsyncMqttClient is async-native and avoids watchdog timer resets.
- **REST + MQTT (both):** REST is natural for the web dashboard and direct control. MQTT is natural for HA and real-time state sync. Each does what it's best at.
- **Calendar via HA, not direct:** Avoids OAuth token management on embedded. HA already has calendar integrations — let it handle the complexity and push simple JSON via MQTT.
- **Manual always wins:** Simple, predictable priority rule. No complex override logic. Auto-expire resets priority so automation can resume.
- **No authentication (intentional):** LAN-only device. Adding auth would complicate the dashboard and API with minimal security benefit on a home network.
- **Prototype font:** The classic 1982 Epcot/World Bold geometric typeface, matching the monorail interior signage. Free for personal use.
- **Geodesic pattern:** Spaceship Earth's triangular facets as a subtle background texture, tinted per mode — ties the Disney inspiration together without being literal.
- **Monorail color system:** Each mode's color personality mirrors a specific WDW monorail, with pattern visibility varying by mode purpose (calm for Focused/Gold, energetic for Available/Green).
- **Progressive downgrade:** API contract designed cleanly so the same dashboard and HA integration work regardless of which ESP32 board is behind it.
- **Write discipline:** LittleFS persistence only on meaningful state changes (not transient data), debounced to max once per 2 seconds, to avoid flash wear.
