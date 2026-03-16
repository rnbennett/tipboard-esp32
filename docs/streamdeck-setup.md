# Tipboard Stream Deck Setup

## Setup Method: macOS Shortcuts + Stream Deck

1. Open **Shortcuts** app
2. Create a new shortcut for each button
3. Add action: **Run Shell Script**
4. Paste the curl command from the tables below
5. In Stream Deck, drag **Shortcuts** action → select the shortcut

## Setup Method: Windows Scripts + Stream Deck

1. Create a `.bat` file for each button (save to a folder like `C:\tipboard-buttons\`)
2. In Stream Deck, drag **System > Open** action → point to the `.bat` file

## Button Commands

### Mode Buttons

| Button | Mac (curl) | Windows (.bat) |
|--------|-----------|----------------|
| Available | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":0}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":0}'"` |
| Focused | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":1}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":1}'"` |
| Meeting | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":2}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":2}'"` |
| Away | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":3}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":3}'"` |
| Custom | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":5}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":5}'"` |
| On Air | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":6}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":6}'"` |

### Timer Buttons

| Button | Mac (curl) | Windows (.bat) |
|--------|-----------|----------------|
| Pomodoro 25m | `curl -s -X POST http://10.0.0.30/api/timer/start -H "Content-Type: application/json" -d '{"type":"pomodoro","work_min":25}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/timer/start -Method Post -ContentType 'application/json' -Body '{\"type\":\"pomodoro\",\"work_min\":25}'"` |
| Stop Timer | `curl -s -X POST http://10.0.0.30/api/timer/stop` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/timer/stop -Method Post"` |

### Utility Buttons

| Button | Mac (curl) | Windows (.bat) |
|--------|-----------|----------------|
| Bright 100% | `curl -s -X PUT http://10.0.0.30/api/brightness -H "Content-Type: application/json" -d '{"value":100}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/brightness -Method Put -ContentType 'application/json' -Body '{\"value\":100}'"` |
| Bright 50% | `curl -s -X PUT http://10.0.0.30/api/brightness -H "Content-Type: application/json" -d '{"value":50}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/brightness -Method Put -ContentType 'application/json' -Body '{\"value\":50}'"` |

### Custom Subtitle Buttons

| Button | Mac (curl) | Windows (.bat) |
|--------|-----------|----------------|
| Deep Work | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":1,"subtitle":"Deep Work Session"}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":1,\"subtitle\":\"Deep Work Session\"}'"` |
| Lunch | `curl -s -X PUT http://10.0.0.30/api/status -H "Content-Type: application/json" -d '{"mode":3,"subtitle":"Out to Lunch"}'` | `powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":3,\"subtitle\":\"Out to Lunch\"}'"` |

### Windows .bat File Template

Save each as a `.bat` file (e.g., `available.bat`):

```bat
@echo off
powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":0}'"
```

Then in Stream Deck: **System > Open** → browse to the `.bat` file.

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
