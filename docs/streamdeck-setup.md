# Tipboard Stream Deck Setup

## Plugin
Install **API Ninja** from the Stream Deck Store (free).

## Button Configuration

For each button, add an "API Ninja" action with these settings:

### Mode Buttons

| Button | Method | URL | Body |
|--------|--------|-----|------|
| Available | PUT | `http://10.0.0.30/api/status` | `{"mode":0}` |
| Focused | PUT | `http://10.0.0.30/api/status` | `{"mode":1}` |
| In a Meeting | PUT | `http://10.0.0.30/api/status` | `{"mode":2}` |
| Away | PUT | `http://10.0.0.30/api/status` | `{"mode":3}` |
| Custom | PUT | `http://10.0.0.30/api/status` | `{"mode":5}` |
| On Air | PUT | `http://10.0.0.30/api/status` | `{"mode":6}` |

All mode buttons:
- **Content Type:** `application/json`
- **Method:** PUT

### Timer Buttons

| Button | Method | URL | Body |
|--------|--------|-----|------|
| Pomodoro (25 min) | POST | `http://10.0.0.30/api/timer/start` | `{"type":"pomodoro","work_min":25}` |
| Pomodoro (10 min) | POST | `http://10.0.0.30/api/timer/start` | `{"type":"pomodoro","work_min":10}` |
| Stop Timer | POST | `http://10.0.0.30/api/timer/stop` | (empty) |

### Utility Buttons

| Button | Method | URL | Body |
|--------|--------|-----|------|
| Brightness 100% | PUT | `http://10.0.0.30/api/brightness` | `{"value":100}` |
| Brightness 50% | PUT | `http://10.0.0.30/api/brightness` | `{"value":50}` |
| Brightness 10% | PUT | `http://10.0.0.30/api/brightness` | `{"value":10}` |

### Custom Subtitle Button

| Button | Method | URL | Body |
|--------|--------|-----|------|
| Deep Work | PUT | `http://10.0.0.30/api/status` | `{"mode":1,"subtitle":"Deep Work Session"}` |
| Lunch | PUT | `http://10.0.0.30/api/status` | `{"mode":3,"subtitle":"Out to Lunch"}` |
| Streaming | PUT | `http://10.0.0.30/api/status` | `{"mode":6,"subtitle":"Live on Twitch"}` |

## Button Icons

Use monorail colors for the button backgrounds:
- Available: #00A84E (green)
- Focused: #C7A94E (gold)
- Meeting: #E4272C (red)
- Away: #0065C1 (blue)
- Custom: #00B5B8 (teal)
- On Air: #A8A9AD (silver)

## Notes

- Buttons work instantly — no pairing needed
- Works over WiFi as long as PC and tipboard are on the same network
- If the IP changes, update all button URLs
- The tipboard's IP is shown in the top bar (tap to toggle)
