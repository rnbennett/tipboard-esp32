# Phase 1 — Display & Touch Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete on-device UI — seven status modes with Disney-inspired visuals, touch interaction, timers, Pomodoro state machine, and LittleFS persistence.

**Architecture:** Three new components: `state` (mode/timer/priority logic + LittleFS persistence), `ui` (LVGL screens, themes, geodesic pattern, transitions), and font assets in `main/fonts/`. State component owns the data model; UI component subscribes to state changes via callback. LVGL runs on Core 0 (existing); state changes are queued to the LVGL task via `lv_msg` or a simple flag+mutex pattern.

**Tech Stack:** ESP-IDF v5.5.3, LVGL v9.5.0, cJSON (ESP-IDF built-in), esp_littlefs, FreeRTOS

**Current State (Phase 0):** Working LVGL hello world with display (JD9165 MIPI-DSI), touch (GT911 I2C), backlight, draw buffers (2x 1024x50 PSRAM), tick timer, LVGL task on Core 0.

---

## File Structure

```
components/
  board/                    (existing, unchanged)
  state/
    include/
      state.h               — mode enum, status_state_t struct, public API
      persist.h              — LittleFS mount/save/load API
    state.c                  — state logic: mode change, priority, auto-expire, timer tick
    persist.c                — LittleFS init, JSON serialize/deserialize, debounced write
    CMakeLists.txt
    idf_component.yml        — depends on esp_littlefs
  ui/
    include/
      ui.h                   — public UI API: ui_init(), ui_update_from_state()
      ui_theme.h             — color scheme definitions per mode
    ui_screen.c              — three-zone status screen layout
    ui_theme.c               — LVGL style creation, mode color application
    ui_geodesic.c            — Spaceship Earth geodesic triangle pattern (canvas)
    ui_timer_arc.c           — timer arc widget + countdown/elapsed display
    ui_transitions.c         — slide transition between mode changes
    CMakeLists.txt
    idf_component.yml
main/
  main.c                     — modified: init state + UI, wire callbacks, remove hello world
  fonts/
    font_prototype_bold_48.c — Prototype font at 48px (status name)
    font_prototype_bold_28.c — Prototype font at 28px (smaller headings)
```

**Component dependency graph:**
```
main → state, ui, board, lvgl, esp_timer, esp_psram
state → (esp_littlefs, cJSON — both ESP-IDF built-in/managed)
ui → lvgl, state (read-only access to state struct)
board → driver, esp_lcd, esp_lcd_touch, esp_hw_support
```

---

## Chunk 1: State Management Foundation

### Task 1: Create State Component Scaffold

**Files:**
- Create: `components/state/include/state.h`
- Create: `components/state/state.c`
- Create: `components/state/CMakeLists.txt`
- Create: `components/state/idf_component.yml`

- [ ] **Step 1: Create `components/state/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "state.c" "persist.c"
    INCLUDE_DIRS "include"
    REQUIRES cJSON esp_timer
    PRIV_REQUIRES esp_littlefs
)
```

- [ ] **Step 2: Create `components/state/idf_component.yml`**

```yaml
dependencies:
  joltwallet/esp_littlefs:
    version: "*"
```

- [ ] **Step 3: Create `components/state/include/state.h`**

Define the core data model from the design spec:

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Seven status modes matching the WDW Monorail color fleet */
typedef enum {
    MODE_AVAILABLE = 0,   /* Green — Monorail Green */
    MODE_FOCUSED,         /* Amber/Gold — Monorail Gold */
    MODE_MEETING,         /* Red — Monorail Red */
    MODE_AWAY,            /* Cool Gray — Monorail Silver */
    MODE_POMODORO,        /* Red→Green progression */
    MODE_CUSTOM,          /* Coral/Pink — Monorail Coral */
    MODE_STREAMING,       /* Bright Red/Pink — Monorail Coral (brighter) */
    MODE_COUNT
} status_mode_t;

/* Timer type */
typedef enum {
    TIMER_NONE = 0,
    TIMER_ELAPSED,
    TIMER_COUNTDOWN,
} timer_type_t;

/* Who set this status */
typedef enum {
    SOURCE_MANUAL = 0,    /* Touch screen */
    SOURCE_KEYPAD,        /* BLE keypad */
    SOURCE_API,           /* REST API */
    SOURCE_MQTT,          /* MQTT command */
} status_source_t;

/* Pomodoro phase (only relevant when mode == MODE_POMODORO) */
typedef enum {
    POMO_IDLE = 0,
    POMO_WORK,
    POMO_BREAK,
} pomo_phase_t;

/* Full status state — the single source of truth */
typedef struct {
    status_mode_t mode;
    char subtitle[64];

    /* Timer */
    timer_type_t timer_type;
    int64_t timer_started_at;     /* epoch seconds when timer started, 0 if none */
    int32_t timer_duration_sec;   /* countdown duration, 0 for elapsed */

    /* Auto-expire */
    bool auto_expire_enabled;
    int64_t auto_expire_at;       /* epoch seconds, 0 if disabled */
    status_mode_t auto_expire_revert_to;

    /* Source & priority: 1=manual/keypad (highest), 2=api/mqtt */
    status_source_t source;
    uint8_t priority;

    /* Pomodoro */
    pomo_phase_t pomo_phase;
    int32_t pomo_work_sec;        /* configured work duration */
    int32_t pomo_break_sec;       /* configured break duration */
} status_state_t;

/* Callback type for state change notifications */
typedef void (*state_change_cb_t)(const status_state_t *new_state, void *user_data);

/* Initialize state manager (call once at boot) */
esp_err_t state_init(void);

/* Get read-only pointer to current state */
const status_state_t *state_get(void);

/* Mode labels (uppercase, for display) */
const char *state_mode_label(status_mode_t mode);

/* Change mode (respects priority rules) */
esp_err_t state_set_mode(status_mode_t mode, status_source_t source);

/* Set subtitle text */
esp_err_t state_set_subtitle(const char *text);

/* Timer control */
esp_err_t state_timer_start_elapsed(void);
esp_err_t state_timer_start_countdown(int32_t duration_sec);
esp_err_t state_timer_stop(void);

/* Get current timer value in seconds (elapsed: seconds since start, countdown: seconds remaining) */
int32_t state_timer_get_seconds(void);

/* Pomodoro control */
esp_err_t state_pomodoro_start(int32_t work_sec, int32_t break_sec);
esp_err_t state_pomodoro_cancel(void);

/* Auto-expire */
esp_err_t state_set_auto_expire(int32_t duration_sec, status_mode_t revert_to);
esp_err_t state_clear_auto_expire(void);

/* Register callback for state changes (UI uses this) */
esp_err_t state_register_change_cb(state_change_cb_t cb, void *user_data);

/* Called periodically (e.g., every 1s) to check auto-expire and pomodoro transitions */
void state_tick(void);

/* Default mode configuration */
esp_err_t state_set_default_mode(status_mode_t mode);
status_mode_t state_get_default_mode(void);
```

- [ ] **Step 4: Create `components/state/state.c` — core state logic**

```c
#include "state.h"
#include "persist.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "state";

static status_state_t s_state;
static status_mode_t s_default_mode = MODE_AVAILABLE;
static state_change_cb_t s_change_cb = NULL;
static void *s_change_cb_data = NULL;

/* Mode labels (all-caps for display) */
static const char *MODE_LABELS[MODE_COUNT] = {
    "AVAILABLE",
    "FOCUSED",
    "IN A MEETING",
    "AWAY",
    "POMODORO",
    "CUSTOM",
    "ON AIR",
};

static void notify_change(void)
{
    if (s_change_cb) {
        s_change_cb(&s_state, s_change_cb_data);
    }
    persist_save_debounced(&s_state);
}

static uint8_t source_to_priority(status_source_t source)
{
    switch (source) {
    case SOURCE_MANUAL:
    case SOURCE_KEYPAD:
        return 1; /* Highest */
    case SOURCE_API:
    case SOURCE_MQTT:
    default:
        return 2;
    }
}

static int64_t now_epoch_sec(void)
{
    /* Use esp_timer for monotonic time (NTP not available in Phase 1) */
    return (int64_t)(esp_timer_get_time() / 1000000LL);
}

esp_err_t state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = s_default_mode;
    s_state.priority = 2; /* Low priority default so anything can override */
    s_state.pomo_work_sec = 25 * 60;  /* 25 min default */
    s_state.pomo_break_sec = 5 * 60;  /* 5 min default */

    /* Try to restore from LittleFS */
    esp_err_t err = persist_init();
    if (err == ESP_OK) {
        persist_load(&s_state);
        ESP_LOGI(TAG, "Restored state: mode=%d (%s)", s_state.mode, MODE_LABELS[s_state.mode]);
    } else {
        ESP_LOGW(TAG, "Persistence init failed, using defaults");
    }

    return ESP_OK;
}

const status_state_t *state_get(void)
{
    return &s_state;
}

const char *state_mode_label(status_mode_t mode)
{
    if (mode >= MODE_COUNT) return "UNKNOWN";
    return MODE_LABELS[mode];
}

esp_err_t state_set_mode(status_mode_t mode, status_source_t source)
{
    uint8_t incoming_priority = source_to_priority(source);

    /* Manual always wins. Lower priority number = higher priority.
       A new manual action always takes effect regardless of current priority. */
    if (incoming_priority > s_state.priority &&
        source != SOURCE_MANUAL && source != SOURCE_KEYPAD) {
        ESP_LOGW(TAG, "Rejected mode change: current priority %d > incoming %d",
                 s_state.priority, incoming_priority);
        return ESP_ERR_INVALID_STATE;
    }

    /* If switching away from pomodoro, cancel it */
    if (s_state.mode == MODE_POMODORO && mode != MODE_POMODORO) {
        s_state.pomo_phase = POMO_IDLE;
    }

    s_state.mode = mode;
    s_state.source = source;
    s_state.priority = incoming_priority;

    /* Clear auto-expire when manually switching */
    if (source == SOURCE_MANUAL || source == SOURCE_KEYPAD) {
        s_state.auto_expire_enabled = false;
        s_state.auto_expire_at = 0;
    }

    ESP_LOGI(TAG, "Mode changed to %s (source=%d, priority=%d)",
             MODE_LABELS[mode], source, incoming_priority);
    notify_change();
    return ESP_OK;
}

esp_err_t state_set_subtitle(const char *text)
{
    if (!text) {
        s_state.subtitle[0] = '\0';
    } else {
        strncpy(s_state.subtitle, text, sizeof(s_state.subtitle) - 1);
        s_state.subtitle[sizeof(s_state.subtitle) - 1] = '\0';
    }
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_start_elapsed(void)
{
    s_state.timer_type = TIMER_ELAPSED;
    s_state.timer_started_at = now_epoch_sec();
    s_state.timer_duration_sec = 0;
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_start_countdown(int32_t duration_sec)
{
    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_epoch_sec();
    s_state.timer_duration_sec = duration_sec;
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_stop(void)
{
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;
    s_state.timer_duration_sec = 0;
    notify_change();
    return ESP_OK;
}

int32_t state_timer_get_seconds(void)
{
    if (s_state.timer_type == TIMER_NONE) return 0;

    int64_t elapsed = now_epoch_sec() - s_state.timer_started_at;
    if (elapsed < 0) elapsed = 0;

    if (s_state.timer_type == TIMER_ELAPSED) {
        return (int32_t)elapsed;
    } else {
        /* Countdown: remaining seconds */
        int32_t remaining = s_state.timer_duration_sec - (int32_t)elapsed;
        return remaining > 0 ? remaining : 0;
    }
}

esp_err_t state_pomodoro_start(int32_t work_sec, int32_t break_sec)
{
    s_state.mode = MODE_POMODORO;
    s_state.source = SOURCE_MANUAL;
    s_state.priority = 1;
    s_state.pomo_phase = POMO_WORK;
    s_state.pomo_work_sec = work_sec;
    s_state.pomo_break_sec = break_sec;

    /* Start countdown for work period */
    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_epoch_sec();
    s_state.timer_duration_sec = work_sec;

    notify_change();
    return ESP_OK;
}

esp_err_t state_pomodoro_cancel(void)
{
    if (s_state.mode != MODE_POMODORO) return ESP_OK;

    s_state.pomo_phase = POMO_IDLE;
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;

    /* Revert to default mode */
    s_state.mode = s_default_mode;
    s_state.priority = 2; /* Reset priority so automation can take over */

    notify_change();
    return ESP_OK;
}

esp_err_t state_set_auto_expire(int32_t duration_sec, status_mode_t revert_to)
{
    s_state.auto_expire_enabled = true;
    s_state.auto_expire_at = now_epoch_sec() + duration_sec;
    s_state.auto_expire_revert_to = revert_to;
    notify_change();
    return ESP_OK;
}

esp_err_t state_clear_auto_expire(void)
{
    s_state.auto_expire_enabled = false;
    s_state.auto_expire_at = 0;
    notify_change();
    return ESP_OK;
}

esp_err_t state_register_change_cb(state_change_cb_t cb, void *user_data)
{
    s_change_cb = cb;
    s_change_cb_data = user_data;
    return ESP_OK;
}

void state_tick(void)
{
    int64_t now = now_epoch_sec();

    /* Check pomodoro transitions */
    if (s_state.mode == MODE_POMODORO) {
        int32_t remaining = state_timer_get_seconds();

        if (remaining <= 0) {
            if (s_state.pomo_phase == POMO_WORK) {
                /* Work done → transition to break */
                ESP_LOGI(TAG, "Pomodoro: work complete, starting break");
                s_state.pomo_phase = POMO_BREAK;
                s_state.timer_started_at = now;
                s_state.timer_duration_sec = s_state.pomo_break_sec;
                notify_change();
            } else if (s_state.pomo_phase == POMO_BREAK) {
                /* Break done → revert to default */
                ESP_LOGI(TAG, "Pomodoro: break complete, reverting to default");
                s_state.pomo_phase = POMO_IDLE;
                s_state.timer_type = TIMER_NONE;
                s_state.mode = s_default_mode;
                s_state.priority = 2;
                notify_change();
            }
        }
    }

    /* Check auto-expire */
    if (s_state.auto_expire_enabled && now >= s_state.auto_expire_at) {
        ESP_LOGI(TAG, "Auto-expire triggered, reverting to mode %d",
                 s_state.auto_expire_revert_to);
        s_state.auto_expire_enabled = false;
        s_state.mode = s_state.auto_expire_revert_to;
        s_state.priority = 2; /* Reset priority */
        s_state.timer_type = TIMER_NONE;
        notify_change();
    }
}

esp_err_t state_set_default_mode(status_mode_t mode)
{
    s_default_mode = mode;
    return ESP_OK;
}

status_mode_t state_get_default_mode(void)
{
    return s_default_mode;
}
```

- [ ] **Step 5: Build to verify state component compiles**

Run incremental build:
```bash
powershell.exe -Command "& {
  $env:PATH='C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;' + $env:PATH
  Set-Location 'C:\Users\rnben\OneDrive\Documents\Development\tipboard'
  & ninja -C build
}" 2>&1 | tail -20
```
Expected: Compiles without errors (linker may warn about unused functions — that's fine).

- [ ] **Step 6: Commit**

```bash
git add components/state/
git commit -m "feat(state): add state management component with mode/timer/priority logic"
```

---

### Task 2: LittleFS Persistence

**Files:**
- Create: `components/state/include/persist.h`
- Create: `components/state/persist.c`

- [ ] **Step 1: Create `components/state/include/persist.h`**

```c
#pragma once

#include "esp_err.h"
#include "state.h"

/* Mount LittleFS on the "storage" partition */
esp_err_t persist_init(void);

/* Save state to LittleFS as JSON. Debounced: max once per 2 seconds. */
void persist_save_debounced(const status_state_t *state);

/* Force immediate save (used at shutdown or critical moments) */
esp_err_t persist_save_now(const status_state_t *state);

/* Load state from LittleFS. Returns ESP_ERR_NOT_FOUND if no saved state. */
esp_err_t persist_load(status_state_t *state);
```

- [ ] **Step 2: Create `components/state/persist.c`**

```c
#include "persist.h"
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_littlefs.h"
#include "cJSON.h"
#include "esp_timer.h"

static const char *TAG = "persist";
static const char *STATE_PATH = "/storage/state.json";
static const char *CONFIG_PATH = "/storage/config.json";

static int64_t s_last_save_us = 0;
static const int64_t DEBOUNCE_US = 2000000LL; /* 2 seconds in microseconds */
static bool s_mounted = false;

esp_err_t persist_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(err));
        return err;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    ESP_LOGI(TAG, "LittleFS mounted: %zu/%zu bytes used", used, total);

    s_mounted = true;
    return ESP_OK;
}

esp_err_t persist_save_now(const status_state_t *state)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "mode", state->mode);
    cJSON_AddStringToObject(root, "subtitle", state->subtitle);
    cJSON_AddNumberToObject(root, "timer_type", state->timer_type);
    cJSON_AddNumberToObject(root, "timer_started_at", (double)state->timer_started_at);
    cJSON_AddNumberToObject(root, "timer_duration_sec", state->timer_duration_sec);
    cJSON_AddBoolToObject(root, "auto_expire_enabled", state->auto_expire_enabled);
    cJSON_AddNumberToObject(root, "auto_expire_at", (double)state->auto_expire_at);
    cJSON_AddNumberToObject(root, "auto_expire_revert_to", state->auto_expire_revert_to);
    cJSON_AddNumberToObject(root, "source", state->source);
    cJSON_AddNumberToObject(root, "priority", state->priority);
    cJSON_AddNumberToObject(root, "pomo_phase", state->pomo_phase);
    cJSON_AddNumberToObject(root, "pomo_work_sec", state->pomo_work_sec);
    cJSON_AddNumberToObject(root, "pomo_break_sec", state->pomo_break_sec);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(STATE_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", STATE_PATH);
        cJSON_free(json_str);
        return ESP_FAIL;
    }

    fprintf(f, "%s", json_str);
    fclose(f);
    cJSON_free(json_str);

    s_last_save_us = esp_timer_get_time();
    ESP_LOGD(TAG, "State saved to %s", STATE_PATH);
    return ESP_OK;
}

void persist_save_debounced(const status_state_t *state)
{
    if (!s_mounted) return;

    int64_t now = esp_timer_get_time();
    if ((now - s_last_save_us) < DEBOUNCE_US) {
        ESP_LOGD(TAG, "Save debounced (too soon)");
        return;
    }

    persist_save_now(state);
}

esp_err_t persist_load(status_state_t *state)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    struct stat st;
    if (stat(STATE_PATH, &st) != 0) {
        ESP_LOGI(TAG, "No saved state found");
        return ESP_ERR_NOT_FOUND;
    }

    FILE *f = fopen(STATE_PATH, "r");
    if (!f) return ESP_FAIL;

    char *buf = malloc(st.st_size + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }

    size_t read = fread(buf, 1, st.st_size, f);
    fclose(f);
    buf[read] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse saved state JSON");
        return ESP_FAIL;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "mode"))) state->mode = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "subtitle"))) {
        strncpy(state->subtitle, item->valuestring, sizeof(state->subtitle) - 1);
    }
    if ((item = cJSON_GetObjectItem(root, "timer_type"))) state->timer_type = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "timer_started_at"))) state->timer_started_at = (int64_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "timer_duration_sec"))) state->timer_duration_sec = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "auto_expire_enabled"))) state->auto_expire_enabled = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(root, "auto_expire_at"))) state->auto_expire_at = (int64_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "auto_expire_revert_to"))) state->auto_expire_revert_to = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "source"))) state->source = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "priority"))) state->priority = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_phase"))) state->pomo_phase = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_work_sec"))) state->pomo_work_sec = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "pomo_break_sec"))) state->pomo_break_sec = item->valueint;

    cJSON_Delete(root);
    ESP_LOGI(TAG, "State loaded from %s", STATE_PATH);
    return ESP_OK;
}
```

- [ ] **Step 3: Build to verify persistence compiles**

Run incremental build. If cmake cache is stale (new component), do a clean rebuild:
```bash
rm -f sdkconfig && rm -rf build
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```
Expected: Compiles. esp_littlefs component auto-downloads via idf_component.yml.

- [ ] **Step 4: Commit**

```bash
git add components/state/
git commit -m "feat(state): add LittleFS persistence with debounced JSON save/load"
```

---

## Chunk 2: Theme System & Color Schemes

### Task 3: Mode Color Theme Definitions

**Files:**
- Create: `components/ui/include/ui_theme.h`
- Create: `components/ui/ui_theme.c`
- Create: `components/ui/CMakeLists.txt`
- Create: `components/ui/idf_component.yml`

- [ ] **Step 1: Create `components/ui/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "ui_theme.c" "ui_screen.c" "ui_geodesic.c" "ui_timer_arc.c" "ui_transitions.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl state
)
```

Note: List all source files now. Files that don't exist yet can be empty stubs.

- [ ] **Step 2: Create `components/ui/idf_component.yml`**

```yaml
dependencies:
  lvgl/lvgl:
    version: "^9"
```

- [ ] **Step 3: Create empty stub files for later tasks**

Create these as minimal files so CMake doesn't fail:
- `components/ui/ui_screen.c` — `#include "ui.h"` only
- `components/ui/ui_geodesic.c` — empty
- `components/ui/ui_timer_arc.c` — empty
- `components/ui/ui_transitions.c` — empty
- `components/ui/include/ui.h` — header guard only
- `components/ui/include/ui_internal.h` — internal header with declarations for geodesic, timer arc, and transitions:

```c
#pragma once
#include "lvgl.h"
#include "ui_theme.h"

/* ui_geodesic.c */
void ui_geodesic_create(lv_obj_t *parent, int width, int height);
void ui_geodesic_update(lv_obj_t *parent, int width, int height,
                        lv_color_t tint, lv_opa_t opacity);

/* ui_timer_arc.c */
void ui_timer_arc_create(lv_obj_t *parent);
void ui_timer_arc_update(int32_t progress_pct, lv_color_t color, bool visible);

/* ui_transitions.c */
void ui_transition_slide(lv_obj_t *hero, bool slide_left);
```

- [ ] **Step 4: Create `components/ui/include/ui_theme.h`**

```c
#pragma once

#include "lvgl.h"
#include "state.h"

/* Color scheme for a single mode */
typedef struct {
    lv_color_t primary;           /* Main accent (header bar, highlights) */
    lv_color_t primary_dim;       /* Dimmed variant for gradients */
    lv_color_t bg_gradient_start; /* Background gradient start */
    lv_color_t bg_gradient_end;   /* Background gradient end */
    lv_color_t text_primary;      /* Main text color */
    lv_color_t text_secondary;    /* Subtitle/secondary text */
    lv_color_t geo_tint;          /* Geodesic pattern tint */
    uint8_t geo_opacity;          /* Geodesic pattern opacity (0-255) */
} mode_color_scheme_t;

/* Get color scheme for a mode */
const mode_color_scheme_t *ui_theme_get_scheme(status_mode_t mode);

/* Apply mode colors to the current screen */
void ui_theme_apply(status_mode_t mode);

/* Deep cosmic blue-purple base background */
#define UI_COLOR_BG_BASE      lv_color_hex(0x0D0B1A)
#define UI_COLOR_BG_COSMIC    lv_color_hex(0x1A1035)
#define UI_COLOR_DIVIDER      lv_color_hex(0x2A2545)
#define UI_COLOR_TEXT_WHITE    lv_color_hex(0xF0F0F0)
#define UI_COLOR_TEXT_DIM      lv_color_hex(0x8888AA)
```

- [ ] **Step 5: Create `components/ui/ui_theme.c`**

```c
#include "ui_theme.h"

/* WDW Monorail-inspired color schemes per mode */
static const mode_color_scheme_t SCHEMES[MODE_COUNT] = {
    [MODE_AVAILABLE] = {
        .primary           = {.blue = 0x00, .green = 0xCC, .red = 0x44},  /* #44CC00 */
        .primary_dim       = {.blue = 0x00, .green = 0x88, .red = 0x22},
        .bg_gradient_start = {.blue = 0x1A, .green = 0x20, .red = 0x0D},
        .bg_gradient_end   = {.blue = 0x15, .green = 0x35, .red = 0x0A},
        .text_primary      = {.blue = 0xF0, .green = 0xF0, .red = 0xF0},
        .text_secondary    = {.blue = 0x88, .green = 0xCC, .red = 0x88},
        .geo_tint          = {.blue = 0x00, .green = 0xCC, .red = 0x44},
        .geo_opacity        = 38,  /* ~15% — high, welcoming */
    },
    [MODE_FOCUSED] = {
        .primary           = {.blue = 0x00, .green = 0xA8, .red = 0xFF},  /* #FFA800 amber */
        .primary_dim       = {.blue = 0x00, .green = 0x70, .red = 0xCC},
        .bg_gradient_start = {.blue = 0x10, .green = 0x18, .red = 0x1A},
        .bg_gradient_end   = {.blue = 0x08, .green = 0x22, .red = 0x35},
        .text_primary      = {.blue = 0xF0, .green = 0xF0, .red = 0xF0},
        .text_secondary    = {.blue = 0x66, .green = 0xAA, .red = 0xDD},
        .geo_tint          = {.blue = 0x00, .green = 0xA8, .red = 0xFF},
        .geo_opacity        = 25,  /* ~10% — subtle, tone-on-tone calm */
    },
    [MODE_MEETING] = {
        .primary           = {.blue = 0x22, .green = 0x33, .red = 0xEE},  /* #EE3322 red */
        .primary_dim       = {.blue = 0x11, .green = 0x22, .red = 0xAA},
        .bg_gradient_start = {.blue = 0x12, .green = 0x10, .red = 0x1A},
        .bg_gradient_end   = {.blue = 0x08, .green = 0x0A, .red = 0x35},
        .text_primary      = {.blue = 0xF0, .green = 0xF0, .red = 0xF0},
        .text_secondary    = {.blue = 0x88, .green = 0x88, .red = 0xDD},
        .geo_tint          = {.blue = 0x22, .green = 0x33, .red = 0xEE},
        .geo_opacity        = 30,  /* ~12% — medium */
    },
    [MODE_AWAY] = {
        .primary           = {.blue = 0xAA, .green = 0x99, .red = 0x88},  /* #8899AA cool gray */
        .primary_dim       = {.blue = 0x77, .green = 0x66, .red = 0x55},
        .bg_gradient_start = {.blue = 0x18, .green = 0x15, .red = 0x12},
        .bg_gradient_end   = {.blue = 0x22, .green = 0x1E, .red = 0x18},
        .text_primary      = {.blue = 0xDD, .green = 0xDD, .red = 0xDD},
        .text_secondary    = {.blue = 0x99, .green = 0x88, .red = 0x77},
        .geo_tint          = {.blue = 0xAA, .green = 0x99, .red = 0x88},
        .geo_opacity        = 18,  /* ~7% — very quiet */
    },
    [MODE_POMODORO] = {
        .primary           = {.blue = 0x22, .green = 0x33, .red = 0xEE},  /* Starts red */
        .primary_dim       = {.blue = 0x11, .green = 0x22, .red = 0xAA},
        .bg_gradient_start = {.blue = 0x12, .green = 0x10, .red = 0x1A},
        .bg_gradient_end   = {.blue = 0x08, .green = 0x0A, .red = 0x35},
        .text_primary      = {.blue = 0xF0, .green = 0xF0, .red = 0xF0},
        .text_secondary    = {.blue = 0x88, .green = 0x88, .red = 0xDD},
        .geo_tint          = {.blue = 0x22, .green = 0x33, .red = 0xEE},
        .geo_opacity        = 25,  /* Increases with progress */
    },
    [MODE_CUSTOM] = {
        .primary           = {.blue = 0x77, .green = 0x77, .red = 0xFF},  /* #FF7777 coral/pink */
        .primary_dim       = {.blue = 0x44, .green = 0x55, .red = 0xCC},
        .bg_gradient_start = {.blue = 0x15, .green = 0x10, .red = 0x1A},
        .bg_gradient_end   = {.blue = 0x12, .green = 0x0C, .red = 0x35},
        .text_primary      = {.blue = 0xF0, .green = 0xF0, .red = 0xF0},
        .text_secondary    = {.blue = 0xAA, .green = 0x99, .red = 0xEE},
        .geo_tint          = {.blue = 0x77, .green = 0x77, .red = 0xFF},
        .geo_opacity        = 30,  /* ~12% — medium */
    },
    [MODE_STREAMING] = {
        .primary           = {.blue = 0x55, .green = 0x33, .red = 0xFF},  /* #FF3355 bright red/pink */
        .primary_dim       = {.blue = 0x33, .green = 0x22, .red = 0xDD},
        .bg_gradient_start = {.blue = 0x18, .green = 0x0C, .red = 0x1A},
        .bg_gradient_end   = {.blue = 0x12, .green = 0x08, .red = 0x38},
        .text_primary      = {.blue = 0xFF, .green = 0xFF, .red = 0xFF},
        .text_secondary    = {.blue = 0x88, .green = 0x77, .red = 0xFF},
        .geo_tint          = {.blue = 0x55, .green = 0x33, .red = 0xFF},
        .geo_opacity        = 38,  /* ~15% — high, attention-grabbing */
    },
};

const mode_color_scheme_t *ui_theme_get_scheme(status_mode_t mode)
{
    if (mode >= MODE_COUNT) mode = MODE_AVAILABLE;
    return &SCHEMES[mode];
}

void ui_theme_apply(status_mode_t mode)
{
    /* Applied by ui_screen.c when it rebuilds/updates the screen */
    /* This function will be called from ui_screen update logic */
}
```

**Important: lv_color_t initialization:** The struct literal approach above is for visual clarity only. **The implementer MUST replace all struct literals with `lv_color_hex(0xRRGGBB)` calls.** For example, replace `{.blue = 0x00, .green = 0xCC, .red = 0x44}` with `lv_color_hex(0x44CC00)`. The `lv_color_hex()` function is portable across LVGL color depth configurations and handles byte ordering correctly. The hex values in the comments (e.g., `/* #44CC00 */`) are the correct values to use.

- [ ] **Step 6: Build to verify theme compiles**

Run incremental build (or clean rebuild if cmake cache is stale).
Expected: Compiles without errors.

- [ ] **Step 7: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): add theme system with WDW Monorail color schemes for all 7 modes"
```

---

## Chunk 3: Prototype Font Conversion

### Task 4: Convert Prototype Font to LVGL Format

**Files:**
- Create: `main/fonts/font_prototype_bold_48.c` — large status name
- Create: `main/fonts/font_prototype_bold_28.c` — smaller headings
- Modify: `main/CMakeLists.txt` — add font source files

The "Prototype" font is a free recreation of the 1982 Epcot World Bold geometric typeface. It must be converted to LVGL C format using the `lv_font_conv` tool.

- [ ] **Step 1: Download the Prototype font**

The Prototype font (by Production Type / Colophon Foundry, free for personal use) needs to be obtained as a TTF file. Search for "Prototype font Epcot" or "World Bold font." Place the TTF at `tools/fonts/Prototype-Bold.ttf`.

If the exact font cannot be found, **fallback to Montserrat ExtraBold** (already in LVGL) which is geometric and clean. Enable additional Montserrat sizes in sdkconfig:

```ini
# Add to sdkconfig.defaults
CONFIG_LV_FONT_MONTSERRAT_36=y
CONFIG_LV_FONT_MONTSERRAT_40=y
```

- [ ] **Step 2: Install lv_font_conv (Node.js tool)**

```bash
npm install -g lv_font_conv
```

- [ ] **Step 3: Convert font to LVGL C source (48px for status name)**

```bash
lv_font_conv \
  --bpp 4 \
  --size 48 \
  --font tools/fonts/Prototype-Bold.ttf \
  --range 0x20-0x7F \
  --format lvgl \
  --output main/fonts/font_prototype_bold_48.c \
  --lv-include "lvgl.h"
```

This generates a C file declaring `lv_font_t font_prototype_bold_48`.

- [ ] **Step 4: Convert at 28px for smaller headings**

```bash
lv_font_conv \
  --bpp 4 \
  --size 28 \
  --font tools/fonts/Prototype-Bold.ttf \
  --range 0x20-0x7F \
  --format lvgl \
  --output main/fonts/font_prototype_bold_28.c \
  --lv-include "lvgl.h"
```

- [ ] **Step 5: Update `main/CMakeLists.txt` to include font files**

```cmake
idf_component_register(
    SRCS "main.c" "fonts/font_prototype_bold_48.c" "fonts/font_prototype_bold_28.c"
    INCLUDE_DIRS "."
    REQUIRES board state ui lvgl esp_timer esp_psram
)
```

Note: Also adds `state` and `ui` as REQUIRES for upcoming tasks.

- [ ] **Step 6: Build to verify font compiles**

Expected: Compiles. Font data is compiled into the firmware binary.

- [ ] **Step 7: Commit**

```bash
git add main/fonts/ main/CMakeLists.txt
git commit -m "feat(fonts): add Prototype Bold font converted to LVGL at 48px and 28px"
```

**Fallback path:** If the Prototype font can't be obtained, skip Steps 2-4 and use Montserrat 48/36/28/24 which are already available in LVGL. The visual character will be slightly different but the layout and functionality are identical. Update sdkconfig.defaults with additional Montserrat sizes and proceed.

---

## Chunk 4: Status Screen Layout

### Task 5: Three-Zone Status Screen

**Files:**
- Modify: `components/ui/include/ui.h`
- Modify: `components/ui/ui_screen.c`

This builds the main display layout from the design spec: top info bar, hero status zone (~70% screen), bottom event bar.

- [ ] **Step 1: Define public UI API in `components/ui/include/ui.h`**

```c
#pragma once

#include "esp_err.h"
#include "state.h"
#include "lvgl.h"

/* Initialize the UI system. Call after lv_init() and display driver setup. */
esp_err_t ui_init(void);

/* Update UI to reflect current state. Safe to call from LVGL task only. */
void ui_update(const status_state_t *state);

/* Update just the timer display (called every second from LVGL task) */
void ui_update_timer(int32_t seconds, timer_type_t type);

/* Get the main screen object */
lv_obj_t *ui_get_screen(void);
```

- [ ] **Step 2: Build the three-zone layout in `components/ui/ui_screen.c`**

```c
#include "ui.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_screen";

/* Extern font declarations (from main/fonts/) */
LV_FONT_DECLARE(font_prototype_bold_48);
LV_FONT_DECLARE(font_prototype_bold_28);

/* Screen dimensions */
#define SCREEN_W  1024
#define SCREEN_H  600

/* Zone heights */
#define TOP_BAR_H      40
#define BOTTOM_BAR_H   40
#define HEADER_BAR_H   6     /* Glowing color accent bar */
#define HERO_H         (SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H)

/* Screen objects */
static lv_obj_t *s_screen = NULL;

/* Top bar */
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_weather_label = NULL;

/* Hero zone */
static lv_obj_t *s_hero = NULL;
static lv_obj_t *s_header_bar = NULL;       /* Glowing color accent strip */
static lv_obj_t *s_mode_label = NULL;       /* "FOCUSED" — Prototype font */
static lv_obj_t *s_subtitle_label = NULL;   /* "Deep Work Session" — Montserrat */
static lv_obj_t *s_timer_label = NULL;      /* "1:23:45" */

/* Bottom bar */
static lv_obj_t *s_bottom_bar = NULL;
static lv_obj_t *s_event_label = NULL;

/* Divider lines */
static lv_obj_t *s_divider_top = NULL;
static lv_obj_t *s_divider_bottom = NULL;

/* Current mode for tracking changes */
static status_mode_t s_current_mode = MODE_COUNT; /* Invalid initial value */

static void create_top_bar(lv_obj_t *parent)
{
    s_top_bar = lv_obj_create(parent);
    lv_obj_set_size(s_top_bar, SCREEN_W, TOP_BAR_H);
    lv_obj_set_pos(s_top_bar, 0, 0);
    lv_obj_set_style_bg_color(s_top_bar, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_top_bar, 0, 0);
    lv_obj_set_style_radius(s_top_bar, 0, 0);
    lv_obj_set_style_pad_left(s_top_bar, 16, 0);
    lv_obj_set_style_pad_right(s_top_bar, 16, 0);
    lv_obj_set_scrollbar_mode(s_top_bar, LV_SCROLLBAR_MODE_OFF);

    /* Time/date label (left) — placeholder until NTP in Phase 2 */
    s_time_label = lv_label_create(s_top_bar);
    lv_label_set_text(s_time_label, "--:-- . ---");
    lv_obj_set_style_text_color(s_time_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_24, 0);
    lv_obj_align(s_time_label, LV_ALIGN_LEFT_MID, 0, 0);

    /* Weather label (right) — placeholder until Phase 2 */
    s_weather_label = lv_label_create(s_top_bar);
    lv_label_set_text(s_weather_label, "");
    lv_obj_set_style_text_color(s_weather_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_weather_label, &lv_font_montserrat_24, 0);
    lv_obj_align(s_weather_label, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void create_hero_zone(lv_obj_t *parent)
{
    s_hero = lv_obj_create(parent);
    lv_obj_set_size(s_hero, SCREEN_W, HERO_H);
    lv_obj_set_pos(s_hero, 0, TOP_BAR_H);
    lv_obj_set_style_bg_color(s_hero, UI_COLOR_BG_COSMIC, 0);
    lv_obj_set_style_bg_opa(s_hero, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_hero, 0, 0);
    lv_obj_set_style_radius(s_hero, 0, 0);
    lv_obj_set_style_pad_all(s_hero, 0, 0);
    lv_obj_set_scrollbar_mode(s_hero, LV_SCROLLBAR_MODE_OFF);

    /* Glowing header accent bar (full width, at top of hero) */
    s_header_bar = lv_obj_create(s_hero);
    lv_obj_set_size(s_header_bar, SCREEN_W, HEADER_BAR_H);
    lv_obj_set_pos(s_header_bar, 0, 0);
    lv_obj_set_style_bg_color(s_header_bar, lv_color_hex(0x44CC00), 0); /* Default green */
    lv_obj_set_style_bg_opa(s_header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_header_bar, 0, 0);
    lv_obj_set_style_radius(s_header_bar, 0, 0);
    lv_obj_set_style_shadow_width(s_header_bar, 20, 0);
    lv_obj_set_style_shadow_color(s_header_bar, lv_color_hex(0x44CC00), 0);
    lv_obj_set_style_shadow_opa(s_header_bar, LV_OPA_50, 0);
    lv_obj_set_style_shadow_spread(s_header_bar, 4, 0);

    /* Mode name label — Prototype font, ALL CAPS, wide letter-spacing */
    s_mode_label = lv_label_create(s_hero);
    lv_label_set_text(s_mode_label, "AVAILABLE");
    lv_obj_set_style_text_font(s_mode_label, &font_prototype_bold_48, 0);
    lv_obj_set_style_text_color(s_mode_label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_letter_space(s_mode_label, 8, 0);
    lv_obj_align(s_mode_label, LV_ALIGN_CENTER, 0, -60);

    /* Subtitle label — Montserrat, lighter weight */
    s_subtitle_label = lv_label_create(s_hero);
    lv_label_set_text(s_subtitle_label, "");
    lv_obj_set_style_text_font(s_subtitle_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_subtitle_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_subtitle_label, LV_ALIGN_CENTER, 0, 10);

    /* Timer display */
    s_timer_label = lv_label_create(s_hero);
    lv_label_set_text(s_timer_label, "");
    lv_obj_set_style_text_font(s_timer_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_timer_label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_opa(s_timer_label, LV_OPA_80, 0);
    lv_obj_align(s_timer_label, LV_ALIGN_CENTER, 0, 80);
}

static void create_bottom_bar(lv_obj_t *parent)
{
    s_bottom_bar = lv_obj_create(parent);
    lv_obj_set_size(s_bottom_bar, SCREEN_W, BOTTOM_BAR_H);
    lv_obj_set_pos(s_bottom_bar, 0, SCREEN_H - BOTTOM_BAR_H);
    lv_obj_set_style_bg_color(s_bottom_bar, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bottom_bar, 0, 0);
    lv_obj_set_style_radius(s_bottom_bar, 0, 0);
    lv_obj_set_style_pad_left(s_bottom_bar, 16, 0);
    lv_obj_set_style_pad_right(s_bottom_bar, 16, 0);
    lv_obj_set_scrollbar_mode(s_bottom_bar, LV_SCROLLBAR_MODE_OFF);

    /* Next event label — placeholder until MQTT in Phase 3 */
    s_event_label = lv_label_create(s_bottom_bar);
    lv_label_set_text(s_event_label, "");
    lv_obj_set_style_text_color(s_event_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_event_label, &lv_font_montserrat_24, 0);
    lv_obj_align(s_event_label, LV_ALIGN_LEFT_MID, 0, 0);
}

static void create_dividers(lv_obj_t *parent)
{
    /* Top divider: between top bar and hero */
    s_divider_top = lv_obj_create(parent);
    lv_obj_set_size(s_divider_top, SCREEN_W, 1);
    lv_obj_set_pos(s_divider_top, 0, TOP_BAR_H);
    lv_obj_set_style_bg_color(s_divider_top, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(s_divider_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_divider_top, 0, 0);
    lv_obj_set_style_radius(s_divider_top, 0, 0);

    /* Bottom divider: between hero and bottom bar */
    s_divider_bottom = lv_obj_create(parent);
    lv_obj_set_size(s_divider_bottom, SCREEN_W, 1);
    lv_obj_set_pos(s_divider_bottom, 0, SCREEN_H - BOTTOM_BAR_H);
    lv_obj_set_style_bg_color(s_divider_bottom, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(s_divider_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_divider_bottom, 0, 0);
    lv_obj_set_style_radius(s_divider_bottom, 0, 0);
}

esp_err_t ui_init(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    create_top_bar(s_screen);
    create_hero_zone(s_screen);
    create_bottom_bar(s_screen);
    create_dividers(s_screen);

    lv_scr_load(s_screen);

    ESP_LOGI(TAG, "UI initialized: 3-zone layout on %dx%d", SCREEN_W, SCREEN_H);
    return ESP_OK;
}

void ui_update(const status_state_t *state)
{
    if (!state || !s_screen) return;

    const mode_color_scheme_t *scheme = ui_theme_get_scheme(state->mode);

    /* Update mode label */
    lv_label_set_text(s_mode_label, state_mode_label(state->mode));

    /* Update subtitle */
    if (state->subtitle[0] != '\0') {
        lv_label_set_text(s_subtitle_label, state->subtitle);
        lv_obj_remove_flag(s_subtitle_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_subtitle_label, LV_OBJ_FLAG_HIDDEN);
    }

    /* Update header bar color + glow */
    lv_obj_set_style_bg_color(s_header_bar, scheme->primary, 0);
    lv_obj_set_style_shadow_color(s_header_bar, scheme->primary, 0);

    /* Update hero background gradient */
    lv_obj_set_style_bg_color(s_hero, scheme->bg_gradient_start, 0);
    lv_obj_set_style_bg_grad_color(s_hero, scheme->bg_gradient_end, 0);
    lv_obj_set_style_bg_grad_dir(s_hero, LV_GRAD_DIR_VER, 0);

    /* Update text colors */
    lv_obj_set_style_text_color(s_mode_label, scheme->text_primary, 0);
    lv_obj_set_style_text_color(s_subtitle_label, scheme->text_secondary, 0);

    /* Update divider colors to match mode (subtle) */
    lv_obj_set_style_bg_color(s_divider_top, scheme->primary_dim, 0);
    lv_obj_set_style_bg_opa(s_divider_top, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(s_divider_bottom, scheme->primary_dim, 0);
    lv_obj_set_style_bg_opa(s_divider_bottom, LV_OPA_30, 0);

    s_current_mode = state->mode;
}

void ui_update_timer(int32_t seconds, timer_type_t type)
{
    if (!s_timer_label) return;

    if (type == TIMER_NONE) {
        lv_label_set_text(s_timer_label, "");
        return;
    }
    /* For countdown, hide when expired. For elapsed, always show (even at 0). */
    if (type == TIMER_COUNTDOWN && seconds <= 0) {
        lv_label_set_text(s_timer_label, "");
        return;
    }

    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;

    if (h > 0) {
        lv_label_set_text_fmt(s_timer_label, "%d:%02d:%02d", h, m, s);
    } else {
        lv_label_set_text_fmt(s_timer_label, "%d:%02d", m, s);
    }
}

lv_obj_t *ui_get_screen(void)
{
    return s_screen;
}
```

- [ ] **Step 3: Update `main/main.c` — wire up state + UI (replace hello world)**

Replace the hello world UI creation with:

```c
#include "state.h"
#include "ui.h"

/* In app_main(), after LVGL display + input device setup: */

/* Initialize state manager (loads from LittleFS) */
ESP_ERROR_CHECK(state_init());

/* Initialize UI (creates status screen) */
ESP_ERROR_CHECK(ui_init());

/* Apply initial state to UI */
ui_update(state_get());

/* Register state change callback to update UI */
state_register_change_cb(on_state_change, NULL);

/* Start 1-second tick timer for state_tick() and timer display updates */
```

The full main.c rewrite is in Task 11 (integration). For now, ensure the build succeeds with the new components wired in.

- [ ] **Step 4: Build to verify screen layout compiles**

Clean rebuild needed (new component dependencies):
```bash
rm -f sdkconfig && rm -rf build
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```
Expected: Compiles and flashes. Screen shows three-zone layout with default "AVAILABLE" mode.

- [ ] **Step 5: Verify on hardware**

Flash and check:
- Dark cosmic background visible
- Green header accent bar at top of hero zone with glow effect
- "AVAILABLE" text centered in hero zone (Prototype font or Montserrat fallback)
- Top bar and bottom bar visible but with placeholder text
- Thin divider lines between zones

- [ ] **Step 6: Commit**

```bash
git add components/ui/ main/
git commit -m "feat(ui): three-zone status screen layout with mode colors and timer display"
```

---

## Chunk 5: Geodesic Pattern Background

### Task 6: Spaceship Earth Geodesic Triangle Pattern

**Files:**
- Modify: `components/ui/ui_geodesic.c`
- Modify: `components/ui/ui_screen.c` (add geodesic layer to hero zone)

The geodesic pattern is a low-opacity overlay of triangular facets (inspired by Spaceship Earth) that covers the hero zone background. It's tinted in the current mode's accent color.

- [ ] **Step 1: Implement geodesic pattern in `components/ui/ui_geodesic.c`**

Use LVGL's canvas or line drawing to render a repeating geodesic triangle grid. The pattern is rendered once per mode change (when the tint color changes), not every frame.

```c
#include "ui_theme.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui_geo";

/* Geodesic triangle grid parameters */
#define GEO_CELL_W    80    /* Triangle cell width */
#define GEO_CELL_H    70    /* Triangle cell height */
#define GEO_LINE_W    1     /* Line width */

static lv_obj_t *s_geo_canvas = NULL;
static uint8_t *s_canvas_buf = NULL;

void ui_geodesic_create(lv_obj_t *parent, int width, int height)
{
    /* Use an LVGL canvas for the geodesic overlay.
     * Canvas buffer allocated from PSRAM for the hero zone size. */
    /* Alternative approach: use lv_obj lines for lower memory usage.
     * The implementer should choose based on available PSRAM. */

    /* Simple approach: create line objects for the triangle grid */
    /* This uses minimal memory and renders efficiently via LVGL */

    ESP_LOGI(TAG, "Creating geodesic pattern %dx%d", width, height);

    /* We'll add lines directly to the parent (hero zone).
     * They sit below the text labels due to creation order. */
}

void ui_geodesic_update(lv_obj_t *parent, int width, int height,
                        lv_color_t tint, lv_opa_t opacity)
{
    /* Remove old geodesic lines and redraw with new tint.
     * Called on mode change. */

    /* Simple line-based geodesic grid:
     * Row pattern alternates between upward and downward pointing triangles.
     *
     *   /\    /\    /\
     *  /  \  /  \  /  \
     * /____\/____\/____\
     * \    /\    /\    /
     *  \  /  \  /  \  /
     *   \/    \/    \/
     */

    int cols = (width / GEO_CELL_W) + 2;
    int rows = (height / GEO_CELL_H) + 2;

    /* Allocate persistent point arrays for all triangles.
     * Each line object needs its OWN point array (not shared static).
     * Allocate from heap once; reuse across mode changes by updating color only. */
    int total_tris = rows * cols;
    lv_point_precise_t (*all_pts)[4] = heap_caps_malloc(
        total_tris * sizeof(lv_point_precise_t[4]), MALLOC_CAP_SPIRAM);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int idx = row * cols + col;
            int x = col * GEO_CELL_W;
            int y = row * GEO_CELL_H;

            /* Upward triangle */
            all_pts[idx][0].x = x;                   all_pts[idx][0].y = y + GEO_CELL_H;
            all_pts[idx][1].x = x + GEO_CELL_W / 2;  all_pts[idx][1].y = y;
            all_pts[idx][2].x = x + GEO_CELL_W;      all_pts[idx][2].y = y + GEO_CELL_H;
            all_pts[idx][3].x = x;                   all_pts[idx][3].y = y + GEO_CELL_H;

            lv_obj_t *line_up = lv_line_create(parent);
            lv_line_set_points(line_up, all_pts[idx], 4);
            lv_obj_set_style_line_color(line_up, tint, 0);
            lv_obj_set_style_line_opa(line_up, opacity, 0);
            lv_obj_set_style_line_width(line_up, GEO_LINE_W, 0);
        }
    }
}
```

**Important implementation note:** The above is a conceptual sketch. The actual implementation must handle:
1. **Memory management** — Don't create new line objects on every mode change. Either reuse a fixed set of line objects (update their colors), or delete and recreate. Reuse is strongly preferred.
2. **Static point arrays** — `lv_point_precise_t` arrays must be static or heap-allocated (not stack) since LVGL references them.
3. **Z-order** — Geodesic lines must be created before text labels so they render behind text.

The implementer should create all line objects once in `ui_geodesic_create()`, store them in a static array, and only update colors/opacity in `ui_geodesic_update()`.

- [ ] **Step 2: Integrate geodesic into hero zone**

In `ui_screen.c`, in `create_hero_zone()`, call geodesic creation BEFORE text labels:
```c
/* After creating s_hero, before s_header_bar: */
ui_geodesic_create(s_hero, SCREEN_W, HERO_H);
```

In `ui_update()`, update geodesic tint when mode changes:
```c
if (state->mode != s_current_mode) {
    ui_geodesic_update(s_hero, SCREEN_W, HERO_H,
                       scheme->geo_tint, scheme->geo_opacity);
}
```

Add to `components/ui/ui_geodesic.c` header or in `ui_theme.h`:
```c
void ui_geodesic_create(lv_obj_t *parent, int width, int height);
void ui_geodesic_update(lv_obj_t *parent, int width, int height,
                        lv_color_t tint, lv_opa_t opacity);
```

- [ ] **Step 3: Build and flash**

Expected: Geodesic triangle pattern visible behind status text at low opacity, tinted green (default Available mode).

- [ ] **Step 4: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): add Spaceship Earth geodesic triangle pattern overlay"
```

---

## Chunk 6: Touch Interaction & Slide Transitions

### Task 7: Touch-to-Cycle Modes

**Files:**
- Modify: `components/ui/ui_screen.c` — add tap event handler to hero zone
- Modify: `components/ui/ui_transitions.c` — slide animation

- [ ] **Step 1: Add tap handler to cycle modes in `ui_screen.c`**

In `create_hero_zone()`, add click event to the hero zone:

```c
lv_obj_add_event_cb(s_hero, hero_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
lv_obj_add_flag(s_hero, LV_OBJ_FLAG_CLICKABLE);
```

Implement the callback:
```c
static void hero_tap_cb(lv_event_t *e)
{
    const status_state_t *state = state_get();
    status_mode_t next = (state->mode + 1) % MODE_COUNT;

    /* Skip POMODORO in tap cycle (it's started explicitly) */
    if (next == MODE_POMODORO) {
        next = (next + 1) % MODE_COUNT;
    }

    state_set_mode(next, SOURCE_MANUAL);
}
```

- [ ] **Step 2: Add swipe gesture for left/right mode cycling**

```c
/* In create_hero_zone(): */
lv_obj_add_event_cb(s_hero, hero_gesture_cb, LV_EVENT_GESTURE, NULL);

/* Callback: */
static void hero_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    const status_state_t *state = state_get();
    status_mode_t next;

    if (dir == LV_DIR_LEFT) {
        next = (state->mode + 1) % MODE_COUNT;
        if (next == MODE_POMODORO) next = (next + 1) % MODE_COUNT;
    } else if (dir == LV_DIR_RIGHT) {
        next = (state->mode + MODE_COUNT - 1) % MODE_COUNT;
        if (next == MODE_POMODORO) next = (next + MODE_COUNT - 1) % MODE_COUNT;
    } else {
        return;
    }

    state_set_mode(next, SOURCE_MANUAL);
}
```

- [ ] **Step 3: Implement slide transition in `ui_transitions.c`**

```c
#include "lvgl.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "ui_trans";

#define SLIDE_DURATION_MS  300

/* Slide the hero zone content for mode transitions.
 * New content slides in from the right, old slides out left (or vice versa). */

static lv_anim_t s_slide_anim;
static bool s_animating = false;

static void slide_anim_cb(void *var, int32_t val)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_style_translate_x(obj, val, 0);
}

static void slide_anim_done_cb(lv_anim_t *a)
{
    s_animating = false;
}

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    if (s_animating) return;
    s_animating = true;

    int start = slide_left ? 0 : 0;
    int mid = slide_left ? -50 : 50;

    /* Quick slide out and back for a subtle transition feel */
    lv_anim_init(&s_slide_anim);
    lv_anim_set_var(&s_slide_anim, hero);
    lv_anim_set_values(&s_slide_anim, mid, 0);
    lv_anim_set_time(&s_slide_anim, SLIDE_DURATION_MS);
    lv_anim_set_exec_cb(&s_slide_anim, slide_anim_cb);
    lv_anim_set_completed_cb(&s_slide_anim, slide_anim_done_cb);
    lv_anim_set_path_cb(&s_slide_anim, lv_anim_path_ease_out);
    lv_anim_start(&s_slide_anim);
}
```

Add declaration to a header (in `ui.h` or internal header):
```c
void ui_transition_slide(lv_obj_t *hero, bool slide_left);
```

- [ ] **Step 4: Trigger slide on mode change**

In `ui_update()` in `ui_screen.c`, when mode changes:
```c
if (state->mode != s_current_mode) {
    /* Determine slide direction */
    bool slide_left = (state->mode > s_current_mode);
    ui_transition_slide(s_hero, slide_left);
    /* ... update colors and labels ... */
}
```

- [ ] **Step 5: Build and flash**

Expected: Tapping hero zone cycles through modes with a slide animation. Swiping left/right also cycles. Each mode shows different colors, label text, and header bar color.

- [ ] **Step 6: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): add tap/swipe mode cycling with slide transitions"
```

---

### Task 8: Long-Press Placeholder

**Files:**
- Modify: `components/ui/ui_screen.c`

- [ ] **Step 1: Add long-press handler (future settings overlay)**

```c
/* In create_hero_zone(): */
lv_obj_add_event_cb(s_hero, hero_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);

static void hero_longpress_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Long press detected — settings overlay (Phase 2+)");
    /* TODO: Phase 2 — show settings overlay */
}
```

- [ ] **Step 2: Build and verify**

Expected: Long press logs a message but doesn't crash. No UI change yet.

- [ ] **Step 3: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): add long-press placeholder for future settings overlay"
```

---

## Chunk 7: Timer System & Pomodoro

### Task 9: Timer Arc Widget

**Files:**
- Modify: `components/ui/ui_timer_arc.c`
- Modify: `components/ui/ui_screen.c` — integrate arc into hero zone

- [ ] **Step 1: Implement timer arc in `ui_timer_arc.c`**

An arc widget that shows progress for countdown timers and Pomodoro. For elapsed timers, it's hidden (just the time text is shown).

```c
#include "lvgl.h"
#include "ui_theme.h"
#include "esp_log.h"

static const char *TAG = "ui_arc";

static lv_obj_t *s_arc = NULL;

void ui_timer_arc_create(lv_obj_t *parent)
{
    s_arc = lv_arc_create(parent);
    lv_obj_set_size(s_arc, 180, 180);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);

    /* Style: thin arc, mode-colored */
    lv_obj_set_style_arc_width(s_arc, 6, LV_PART_MAIN);       /* Background arc */
    lv_obj_set_style_arc_color(s_arc, UI_COLOR_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_arc, LV_OPA_30, LV_PART_MAIN);

    lv_obj_set_style_arc_width(s_arc, 6, LV_PART_INDICATOR);  /* Progress arc */
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    /* Remove knob */
    lv_obj_set_style_pad_all(s_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Not clickable — display only */
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    /* Initially hidden */
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, 40);
}

void ui_timer_arc_update(int32_t progress_pct, lv_color_t color, bool visible)
{
    if (!s_arc) return;

    if (!visible) {
        lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_value(s_arc, progress_pct);
    lv_obj_set_style_arc_color(s_arc, color, LV_PART_INDICATOR);
}
```

Add declarations:
```c
void ui_timer_arc_create(lv_obj_t *parent);
void ui_timer_arc_update(int32_t progress_pct, lv_color_t color, bool visible);
```

- [ ] **Step 2: Integrate arc into hero zone**

In `ui_screen.c`'s `create_hero_zone()`, after subtitle label:
```c
ui_timer_arc_create(s_hero);
```

In the timer update logic, calculate progress and update arc:
```c
void ui_update_timer(int32_t seconds, timer_type_t type)
{
    /* ... existing timer label code ... */

    /* Update arc for countdown timers */
    const status_state_t *state = state_get();
    if (type == TIMER_COUNTDOWN && state->timer_duration_sec > 0) {
        int32_t pct = 100 - ((seconds * 100) / state->timer_duration_sec);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        const mode_color_scheme_t *scheme = ui_theme_get_scheme(state->mode);
        ui_timer_arc_update(pct, scheme->primary, true);
    } else {
        ui_timer_arc_update(0, lv_color_black(), false);
    }
}
```

- [ ] **Step 3: Reposition timer label inside arc for countdown mode**

When countdown is active, the timer text should appear centered within the arc ring. Adjust alignment:

```c
/* In ui_update_timer: */
if (type == TIMER_COUNTDOWN) {
    lv_obj_align(s_timer_label, LV_ALIGN_CENTER, 0, 40); /* Inside arc center */
    lv_obj_set_style_text_font(s_timer_label, &lv_font_montserrat_36, 0); /* Slightly smaller to fit */
} else {
    lv_obj_align(s_timer_label, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_text_font(s_timer_label, &lv_font_montserrat_48, 0);
}
```

Note: This requires `CONFIG_LV_FONT_MONTSERRAT_36=y` in sdkconfig.defaults.

- [ ] **Step 4: Build and flash**

Expected: Timer arc appears when countdown is active. Shows progress as arc fills. Timer text centered inside arc.

- [ ] **Step 5: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): add timer arc widget for countdown/Pomodoro progress display"
```

---

### Task 10: Pomodoro Touch Trigger & Color Transitions

**Files:**
- Modify: `components/ui/ui_screen.c` — add bottom bar tap to start Pomodoro
- Modify: `components/ui/ui_theme.c` — Pomodoro color interpolation

- [ ] **Step 1: Add Pomodoro start/stop to bottom bar tap**

Repurpose the bottom bar (which has no calendar event yet) as a Pomodoro trigger in Phase 1:

```c
/* In create_bottom_bar(): */
s_pomo_label = lv_label_create(s_bottom_bar);
lv_label_set_text(s_pomo_label, "Tap for Pomodoro (25:00)");
lv_obj_set_style_text_color(s_pomo_label, UI_COLOR_TEXT_DIM, 0);
lv_obj_set_style_text_font(s_pomo_label, &lv_font_montserrat_24, 0);
lv_obj_align(s_pomo_label, LV_ALIGN_CENTER, 0, 0);

lv_obj_add_event_cb(s_bottom_bar, bottom_bar_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
lv_obj_add_flag(s_bottom_bar, LV_OBJ_FLAG_CLICKABLE);
```

```c
static void bottom_bar_tap_cb(lv_event_t *e)
{
    const status_state_t *state = state_get();
    if (state->mode == MODE_POMODORO) {
        state_pomodoro_cancel();
    } else {
        state_pomodoro_start(state->pomo_work_sec, state->pomo_break_sec);
    }
}
```

- [ ] **Step 2: Update Pomodoro label text based on state**

In `ui_update()`:
```c
if (state->mode == MODE_POMODORO) {
    if (state->pomo_phase == POMO_WORK) {
        lv_label_set_text(s_pomo_label, "Working... (tap to cancel)");
    } else if (state->pomo_phase == POMO_BREAK) {
        lv_label_set_text(s_pomo_label, "Break time! (tap to cancel)");
    }
} else {
    lv_label_set_text(s_pomo_label, "Tap for Pomodoro (25:00)");
}
```

- [ ] **Step 3: Implement Pomodoro color transition (red→green)**

The Pomodoro mode shifts from red (work) to green (break). In `ui_theme.c`, add a function to interpolate:

```c
mode_color_scheme_t ui_theme_get_pomo_scheme(pomo_phase_t phase, int32_t progress_pct)
{
    if (phase == POMO_BREAK) {
        /* Green for break */
        return SCHEMES[MODE_AVAILABLE];
    }
    /* Red for work (default Pomodoro scheme) */
    return SCHEMES[MODE_POMODORO];
}
```

In `ui_update()`, when mode is POMODORO, use this instead of the static scheme:
```c
const mode_color_scheme_t *scheme;
mode_color_scheme_t pomo_scheme;
if (state->mode == MODE_POMODORO) {
    pomo_scheme = ui_theme_get_pomo_scheme(state->pomo_phase,
        state->timer_duration_sec > 0
            ? (100 - (state_timer_get_seconds() * 100 / state->timer_duration_sec))
            : 0);
    scheme = &pomo_scheme;
} else {
    scheme = ui_theme_get_scheme(state->mode);
}
```

- [ ] **Step 4: Build and flash**

Expected: Tapping bottom bar starts Pomodoro (25-min work countdown). Arc fills as time progresses. Colors are red during work. When work completes, auto-transitions to break (green, 5-min). When break completes, reverts to default mode.

- [ ] **Step 5: Commit**

```bash
git add components/ui/
git commit -m "feat(ui): Pomodoro touch trigger with work/break color transitions"
```

---

## Chunk 8: Integration & Main Loop Rewrite

### Task 11: Rewrite main.c

**Files:**
- Modify: `main/main.c` — complete rewrite integrating state + UI

- [ ] **Step 1: Rewrite `main/main.c`**

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_psram.h"
#include "lvgl.h"
#include "board.h"
#include "state.h"
#include "ui.h"

static const char *TAG = "main";

static esp_lcd_touch_handle_t s_touch = NULL;

/* ── LVGL callbacks (unchanged from Phase 0) ── */

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (s_touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    esp_lcd_touch_read_data(s_touch);

    esp_lcd_touch_point_data_t tp_data[1];
    uint8_t count = 0;
    esp_lcd_touch_get_data(s_touch, tp_data, &count, 1);

    if (count > 0) {
        data->point.x = tp_data[0].x;
        data->point.y = tp_data[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(2);
}

/* ── State change callback (called from state module) ── */

static void on_state_change(const status_state_t *new_state, void *user_data)
{
    /* UI update must happen in LVGL task context.
     * For Phase 1, state changes originate from touch (which is in LVGL task),
     * so this is safe. For Phase 2+ (API/MQTT on Core 1), we'll need
     * lv_msg or a queue to marshal to LVGL task. */
    ui_update(new_state);
}

/* ── 1-second periodic callback for state tick + timer display ── */
/* IMPORTANT: This runs as an lv_timer inside the LVGL task loop,
 * ensuring all LVGL calls and state_tick() -> notify_change() -> ui_update()
 * happen in the LVGL task context. Thread-safe by design. */

static void one_second_lv_timer_cb(lv_timer_t *timer)
{
    state_tick();

    const status_state_t *state = state_get();
    int32_t seconds = state_timer_get_seconds();
    ui_update_timer(seconds, state->timer_type);
}

/* ── LVGL task (Core 0) ── */

static void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        if (time_till_next < 2) time_till_next = 2;
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

/* ── App main ── */

void app_main(void)
{
    ESP_LOGI(TAG, "Tipboard Phase 1 starting...");

    /* ── Hardware init ── */
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(board_display_init(&panel));
    ESP_ERROR_CHECK(board_backlight_init());
    board_touch_init(&s_touch); /* Non-fatal if touch fails */

    /* ── LVGL init ── */
    lv_init();

    /* Display driver */
    lv_display_t *disp = lv_display_create(1024, 600);
    lv_display_set_user_data(disp, panel);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* Draw buffers from PSRAM */
    size_t buf_size = 1024 * 50 * sizeof(lv_color16_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Input device */
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    /* Tick timer (2ms) */
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

    /* ── State init (loads from LittleFS) ── */
    ESP_ERROR_CHECK(state_init());
    state_register_change_cb(on_state_change, NULL);

    /* ── UI init (creates status screen) ── */
    ESP_ERROR_CHECK(ui_init());
    ui_update(state_get());

    /* ── 1-second LVGL timer for state tick + UI updates ── */
    /* Runs inside the LVGL task loop — safe for all LVGL and state calls */
    lv_timer_create(one_second_lv_timer_cb, 1000, NULL);

    /* ── Backlight on ── */
    board_backlight_set(100);

    /* ── LVGL task on Core 0 ── */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Tipboard Phase 1 running");
}
```

- [ ] **Step 2: Update `main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "main.c" "fonts/font_prototype_bold_48.c" "fonts/font_prototype_bold_28.c"
    INCLUDE_DIRS "."
    REQUIRES board state ui lvgl esp_timer esp_psram
)
```

If fonts aren't ready yet, remove the font SRCS and use Montserrat fallback in ui_screen.c:
```c
/* Replace: lv_obj_set_style_text_font(s_mode_label, &font_prototype_bold_48, 0); */
/* With:    lv_obj_set_style_text_font(s_mode_label, &lv_font_montserrat_48, 0); */
```

- [ ] **Step 3: Update `sdkconfig.defaults` for additional fonts**

Add to sdkconfig.defaults:
```ini
CONFIG_LV_FONT_MONTSERRAT_36=y
CONFIG_LV_FONT_MONTSERRAT_40=y
```

- [ ] **Step 4: Clean rebuild + flash**

New components require cmake reconfiguration:
```bash
rm -f sdkconfig && rm -rf build
powershell.exe -ExecutionPolicy Bypass -File build_flash.ps1 2>&1 | tail -20
```

- [ ] **Step 5: Hardware verification checklist**

Flash and verify ALL of the following on the actual 7" display:

1. **Boot:** Device boots to "AVAILABLE" mode (green) with no crash
2. **Layout:** Three zones visible — top bar (time placeholder), hero (mode name + subtitle), bottom bar (Pomodoro trigger)
3. **Mode cycling:** Tap hero zone cycles through Available → Focused → Meeting → Away → Custom → Streaming → Available (skips Pomodoro)
4. **Color themes:** Each mode shows distinct colors — green, amber, red, gray, coral, bright pink
5. **Header bar:** Glowing colored accent bar changes color with mode
6. **Geodesic pattern:** Low-opacity triangle pattern visible in hero zone, tint matches mode
7. **Swipe:** Swipe left/right in hero zone cycles modes in respective direction
8. **Slide transition:** Mode changes animate with a subtle slide
9. **Pomodoro:** Tap bottom bar starts 25-min work countdown with arc, timer text inside arc
10. **Pomodoro break:** When work timer reaches 0, auto-transitions to 5-min break (green)
11. **Pomodoro revert:** When break timer reaches 0, reverts to default mode (Available)
12. **Pomodoro cancel:** Tap bottom bar during Pomodoro cancels and reverts
13. **Persistence:** Reboot device. It should restore the last active mode.
14. **Long-press:** Long press on hero zone logs message (no crash)
15. **Performance:** UI feels smooth, no visible lag or tearing during transitions

- [ ] **Step 6: Commit**

```bash
git add main/ sdkconfig.defaults
git commit -m "feat: integrate state + UI for Phase 1 — complete status display with touch interaction"
```

---

## Chunk 9: Bug Fixes & Hardware Tuning

### Task 12: Post-Integration Fixes

This task is a buffer for issues discovered during hardware testing. Common embedded gotchas:

- [ ] **Step 1: Fix any LVGL rendering issues**

Common issues:
- **Color banding in gradients:** Increase draw buffer height from 50 to 100 lines (costs ~200KB more PSRAM)
- **Text not centered:** Check `lv_obj_align()` calls, ensure parent has correct padding/size
- **Geodesic lines not visible:** Check opacity values — may need to increase from 25 to 40+
- **Touch not registering on child objects:** Ensure hero zone child labels don't block clicks (add `LV_OBJ_FLAG_EVENT_BUBBLE` or use `lv_obj_add_flag(child, LV_OBJ_FLAG_CLICK_FOCUSABLE)`)

- [ ] **Step 2: Tune colors on hardware**

Colors look different on the JD9165 7" panel vs a monitor. May need to adjust:
- Background gradient darkness (too dark? too light?)
- Header bar glow (shadow width/spread)
- Geodesic opacity per mode
- Text contrast

Compare each mode on hardware and adjust values in `ui_theme.c`.

- [ ] **Step 3: Tune timer update frequency**

The 1-second `state_tick` timer runs from `esp_timer` (ISR context). If `ui_update_timer()` does too much work in ISR context, move it to a FreeRTOS timer or post to a queue.

**Warning:** `esp_timer` callbacks run in a high-priority task, NOT ISR context on ESP32-P4 by default. But they should still be fast. If issues arise, switch to `lv_timer_create()` for the 1-second update (runs in LVGL task context, which is safer for UI updates).

- [ ] **Step 4: Fix any persistence issues**

- Verify `persist_load()` correctly restores mode after reboot
- Check if timer state is meaningful after reboot (elapsed timer reset is expected since boot time changes)
- Ensure LittleFS partition label matches partitions.csv ("storage")

- [ ] **Step 5: Commit all fixes**

```bash
git add -A
git commit -m "fix: post-integration hardware tuning and bug fixes"
```

---

## Summary: Phase 1 Deliverables

When complete, the tipboard will:

1. Display a three-zone status screen (top bar, hero, bottom bar) on the 7" display
2. Show 7 distinct status modes with WDW Monorail color themes
3. Render a Spaceship Earth geodesic triangle pattern behind the status
4. Cycle modes via tap or swipe with slide transitions
5. Display elapsed and countdown timers with an arc progress indicator
6. Run the full Pomodoro state machine (work → break → revert)
7. Persist state to LittleFS and restore on reboot
8. Use the Prototype geometric font (or Montserrat fallback) for status names

**Scope note — layout variants:** The spec mentions building "both layout variants to compare on real hardware." This plan builds the primary three-zone layout only. Once it's running on hardware, if a second variant is desired, it can be added as a separate screen with minimal effort (copy `ui_screen.c`, adjust zone proportions, switch between them with a gesture). This is intentionally deferred to avoid doubling the UI work before validating the primary design.

**What's NOT in Phase 1:**
- WiFi, NTP, weather (Phase 2)
- REST API, web dashboard (Phase 2)
- MQTT, Home Assistant (Phase 3)
- BLE keypad (Phase 4)
- Animated gradient movement, special-case animations (Phase 5)
- Screen dimming/screensaver (Phase 5)
