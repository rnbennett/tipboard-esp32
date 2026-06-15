#include "state.h"
#include "persist.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "state";

static status_state_t s_state;
static status_mode_t s_default_mode = MODE_AVAILABLE;
static state_change_cb_t s_change_cb = NULL;
static void *s_change_cb_data = NULL;

/* Recursive mutex guarding all access to s_state. It is recursive because a
 * mutator may, via notify_change()'s snapshot, re-enter a locked read on the
 * same task. s_state is written from the LVGL, MQTT, httpd, and (indirectly)
 * weather tasks; without this lock a reader on another core (the CYD is a
 * dual-core ESP32) could observe a torn struct or a non-NUL-terminated
 * subtitle. Created in state_init() before any task that touches state runs. */
static SemaphoreHandle_t s_state_mux = NULL;

#define STATE_LOCK()   do { if (s_state_mux) xSemaphoreTakeRecursive(s_state_mux, portMAX_DELAY); } while (0)
#define STATE_UNLOCK() do { if (s_state_mux) xSemaphoreGiveRecursive(s_state_mux); } while (0)

/* Mode labels (all-caps for display) */
static const char *MODE_LABELS[MODE_COUNT] = {
    [MODE_AVAILABLE] = "AVAILABLE",
    [MODE_FOCUSED]   = "FOCUSED",
    [MODE_MEETING]   = "IN A MEETING",
    [MODE_AWAY]      = "AWAY",
    [MODE_POMODORO]  = "POMODORO",
    [MODE_CUSTOM]    = "CUSTOM",
    [MODE_STREAMING] = "ON AIR",
};

/* Take a consistent snapshot under the lock, then run the change callback and
 * persist from the snapshot — never from live s_state — so listeners and the
 * LittleFS write see a coherent struct even if another task mutates meanwhile.
 * Called AFTER the mutator has released the lock (notify does its own locking),
 * so the lock is never held across the callback or the LittleFS I/O. */
static void notify_change(void)
{
    status_state_t snap;
    STATE_LOCK();
    snap = s_state;
    STATE_UNLOCK();

    if (s_change_cb) {
        s_change_cb(&snap, s_change_cb_data);
    }
    persist_save_debounced(&snap);
}

static uint8_t source_to_priority(status_source_t source)
{
    switch (source) {
    case SOURCE_MANUAL:
    case SOURCE_KEYPAD:
    case SOURCE_API:    /* Dashboard/API is user-initiated — same priority as touch */
        return 1;
    case SOURCE_MQTT:   /* MQTT automation is lower priority */
    default:
        return 2;
    }
}

static int64_t now_monotonic_sec(void)
{
    return (int64_t)(esp_timer_get_time() / 1000000LL);
}

/* Clear the running-timer + auto-expire fields. Caller must hold the lock. */
static void clear_timer_locked(void)
{
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;
    s_state.timer_duration_sec = 0;
    s_state.auto_expire_enabled = false;
    s_state.auto_expire_at = 0;
}

esp_err_t state_init(void)
{
    if (!s_state_mux) {
        s_state_mux = xSemaphoreCreateRecursiveMutex();
    }

    STATE_LOCK();
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = s_default_mode;
    s_state.priority = 2; /* Low priority default so anything can override */
    s_state.pomo_work_sec = 25 * 60;
    s_state.pomo_break_sec = 5 * 60;

    /* Try to restore from LittleFS */
    esp_err_t err = persist_init();
    if (err == ESP_OK) {
        persist_load(&s_state);
        /* state_mode_label() guards the index; persist_load() also clamps mode,
         * but don't index MODE_LABELS[] directly with a freshly-loaded value. */
        ESP_LOGI(TAG, "Restored state: mode=%d (%s)", s_state.mode, state_mode_label(s_state.mode));
    } else {
        ESP_LOGW(TAG, "Persistence init failed, using defaults");
    }

    /* Timer state uses monotonic timestamps that don't survive reboots.
     * Clear all timer/pomodoro state — only mode and subtitle are meaningful. */
    clear_timer_locked();
    s_state.pomo_phase = POMO_IDLE;
    if (s_state.mode == MODE_POMODORO) {
        s_state.mode = s_default_mode;
    }

    /* Apply configured pomodoro durations (config_init runs before state_init).
     * Without this, config pomo_work_min/break_min were dead — start always
     * used the hardcoded 25/5 defaults. Timer state is cleared on reboot anyway,
     * so seeding from config (not the restored running-timer value) is correct. */
    const device_config_t *cfg = config_get();
    if (cfg && cfg->pomo_work_min > 0) {
        s_state.pomo_work_sec = cfg->pomo_work_min * 60;
        s_state.pomo_break_sec = cfg->pomo_break_min * 60;
    }
    STATE_UNLOCK();

    return ESP_OK;
}

const status_state_t *state_get(void)
{
    /* NOTE: returns a pointer to live state — only safe for single-field reads
     * on the same task that mutates. Cross-task / multi-field readers must use
     * state_get_copy() to get a coherent, race-free snapshot. */
    return &s_state;
}

void state_get_copy(status_state_t *out)
{
    if (!out) return;
    STATE_LOCK();
    *out = s_state;
    STATE_UNLOCK();
}

const char *state_mode_label(status_mode_t mode)
{
    if (mode >= MODE_COUNT) return "UNKNOWN";
    const device_config_t *cfg = config_get();
    if (cfg && cfg->mode_labels[mode][0]) {
        return cfg->mode_labels[mode];
    }
    return MODE_LABELS[mode];
}

esp_err_t state_set_mode(status_mode_t mode, status_source_t source)
{
    if (mode >= MODE_COUNT) return ESP_ERR_INVALID_ARG;
    uint8_t incoming_priority = source_to_priority(source);

    /* All sources can change mode — priority enforcement removed.
     * Manual touch can always re-override after an MQTT automation change. */
    STATE_LOCK();
    /* If switching away from pomodoro, cancel it */
    if (s_state.mode == MODE_POMODORO && mode != MODE_POMODORO) {
        s_state.pomo_phase = POMO_IDLE;
    }

    /* If switching away from a timed mode, clear the timer */
    if (s_state.mode != mode) {
        clear_timer_locked();
    }

    s_state.mode = mode;
    s_state.source = source;
    s_state.priority = incoming_priority;

    /* Apply default subtitle from config if subtitle was cleared */
    if (s_state.subtitle[0] == '\0') {
        const device_config_t *cfg = config_get();
        if (cfg && cfg->mode_subtitles[mode][0]) {
            strncpy(s_state.subtitle, cfg->mode_subtitles[mode],
                    sizeof(s_state.subtitle) - 1);
            s_state.subtitle[sizeof(s_state.subtitle) - 1] = '\0';
        }
    }
    STATE_UNLOCK();

    ESP_LOGI(TAG, "Mode changed to %s (source=%d, priority=%d)",
             state_mode_label(mode), source, incoming_priority);
    notify_change();
    return ESP_OK;
}

esp_err_t state_set_subtitle(const char *text)
{
    STATE_LOCK();
    if (!text) {
        s_state.subtitle[0] = '\0';
    } else {
        strncpy(s_state.subtitle, text, sizeof(s_state.subtitle) - 1);
        s_state.subtitle[sizeof(s_state.subtitle) - 1] = '\0';
    }
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_start_elapsed(void)
{
    STATE_LOCK();
    s_state.timer_type = TIMER_ELAPSED;
    s_state.timer_started_at = now_monotonic_sec();
    s_state.timer_duration_sec = 0;
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_start_countdown(int32_t duration_sec)
{
    STATE_LOCK();
    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_monotonic_sec();
    s_state.timer_duration_sec = duration_sec;
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_stop(void)
{
    STATE_LOCK();
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;
    s_state.timer_duration_sec = 0;
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

int32_t state_timer_get_seconds(void)
{
    STATE_LOCK();
    timer_type_t type = s_state.timer_type;
    int64_t started = s_state.timer_started_at;
    int32_t duration = s_state.timer_duration_sec;
    STATE_UNLOCK();

    if (type == TIMER_NONE) return 0;

    int64_t elapsed = now_monotonic_sec() - started;
    if (elapsed < 0) elapsed = 0;

    if (type == TIMER_ELAPSED) {
        return (int32_t)elapsed;
    } else {
        int32_t remaining = duration - (int32_t)elapsed;
        return remaining > 0 ? remaining : 0;
    }
}

esp_err_t state_pomodoro_start(int32_t work_sec, int32_t break_sec)
{
    STATE_LOCK();
    s_state.mode = MODE_POMODORO;
    s_state.source = SOURCE_MANUAL;
    s_state.priority = 1;
    s_state.pomo_phase = POMO_WORK;
    s_state.pomo_work_sec = work_sec;
    s_state.pomo_break_sec = break_sec;

    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_monotonic_sec();
    s_state.timer_duration_sec = work_sec;
    STATE_UNLOCK();

    notify_change();
    return ESP_OK;
}

esp_err_t state_pomodoro_cancel(void)
{
    STATE_LOCK();
    if (s_state.mode != MODE_POMODORO) {
        STATE_UNLOCK();
        return ESP_OK;
    }
    s_state.pomo_phase = POMO_IDLE;
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;

    /* Revert to default mode, reset priority */
    s_state.mode = s_default_mode;
    s_state.priority = 2;
    STATE_UNLOCK();

    notify_change();
    return ESP_OK;
}

esp_err_t state_set_auto_expire(int32_t duration_sec, status_mode_t revert_to)
{
    if (revert_to >= MODE_COUNT) return ESP_ERR_INVALID_ARG;
    STATE_LOCK();
    s_state.auto_expire_enabled = true;
    s_state.auto_expire_at = now_monotonic_sec() + duration_sec;
    s_state.auto_expire_revert_to = revert_to;
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

esp_err_t state_clear_auto_expire(void)
{
    STATE_LOCK();
    s_state.auto_expire_enabled = false;
    s_state.auto_expire_at = 0;
    STATE_UNLOCK();
    notify_change();
    return ESP_OK;
}

esp_err_t state_register_change_cb(state_change_cb_t cb, void *user_data)
{
    s_change_cb = cb;
    s_change_cb_data = user_data;
    return ESP_OK;
}

void state_notify_change(void)
{
    notify_change();
}

void state_tick(void)
{
    int64_t now = now_monotonic_sec();
    bool changed = false;

    STATE_LOCK();
    /* Check pomodoro transitions */
    if (s_state.mode == MODE_POMODORO) {
        int32_t remaining = state_timer_get_seconds();  /* recursive lock — safe */

        if (remaining <= 0) {
            if (s_state.pomo_phase == POMO_WORK) {
                ESP_LOGI(TAG, "Pomodoro: work complete, starting break");
                s_state.pomo_phase = POMO_BREAK;
                s_state.timer_started_at = now;
                s_state.timer_duration_sec = s_state.pomo_break_sec;
                changed = true;
            } else if (s_state.pomo_phase == POMO_BREAK) {
                ESP_LOGI(TAG, "Pomodoro: break complete, reverting to default");
                s_state.pomo_phase = POMO_IDLE;
                s_state.timer_type = TIMER_NONE;
                s_state.mode = s_default_mode;
                s_state.priority = 2;
                changed = true;
            }
        }
    }

    /* Check auto-expire */
    if (s_state.auto_expire_enabled && now >= s_state.auto_expire_at) {
        ESP_LOGI(TAG, "Auto-expire triggered, reverting to mode %d",
                 s_state.auto_expire_revert_to);
        s_state.auto_expire_enabled = false;
        s_state.auto_expire_at = 0;
        s_state.mode = s_state.auto_expire_revert_to;
        s_state.priority = 2;
        clear_timer_locked();
        s_state.subtitle[0] = '\0';
        changed = true;
    }
    STATE_UNLOCK();

    if (changed) notify_change();
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
