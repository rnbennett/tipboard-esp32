#include "state.h"
#include "persist.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "state";

static status_state_t s_state;
static status_mode_t s_default_mode = MODE_AVAILABLE;
static state_change_cb_t s_change_cb = NULL;
static void *s_change_cb_data = NULL;

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

esp_err_t state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = s_default_mode;
    s_state.priority = 2; /* Low priority default so anything can override */
    s_state.pomo_work_sec = 25 * 60;
    s_state.pomo_break_sec = 5 * 60;

    /* Try to restore from LittleFS */
    esp_err_t err = persist_init();
    if (err == ESP_OK) {
        persist_load(&s_state);
        ESP_LOGI(TAG, "Restored state: mode=%d (%s)", s_state.mode, MODE_LABELS[s_state.mode]);
    } else {
        ESP_LOGW(TAG, "Persistence init failed, using defaults");
    }

    /* Timer state uses monotonic timestamps that don't survive reboots.
     * Clear all timer/pomodoro state — only mode and subtitle are meaningful. */
    s_state.timer_type = TIMER_NONE;
    s_state.timer_started_at = 0;
    s_state.timer_duration_sec = 0;
    s_state.pomo_phase = POMO_IDLE;
    s_state.auto_expire_enabled = false;
    s_state.auto_expire_at = 0;
    if (s_state.mode == MODE_POMODORO) {
        s_state.mode = s_default_mode;
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

    /* All sources can change mode — priority enforcement removed.
     * Manual touch can always re-override after an MQTT automation change. */

    /* If switching away from pomodoro, cancel it */
    if (s_state.mode == MODE_POMODORO && mode != MODE_POMODORO) {
        s_state.pomo_phase = POMO_IDLE;
    }

    /* If switching away from a timed mode, clear the timer */
    if (s_state.mode != mode) {
        s_state.timer_type = TIMER_NONE;
        s_state.timer_started_at = 0;
        s_state.timer_duration_sec = 0;
        s_state.auto_expire_enabled = false;
        s_state.auto_expire_at = 0;
    }

    s_state.mode = mode;
    s_state.source = source;
    s_state.priority = incoming_priority;

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
    s_state.timer_started_at = now_monotonic_sec();
    s_state.timer_duration_sec = 0;
    notify_change();
    return ESP_OK;
}

esp_err_t state_timer_start_countdown(int32_t duration_sec)
{
    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_monotonic_sec();
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

    int64_t elapsed = now_monotonic_sec() - s_state.timer_started_at;
    if (elapsed < 0) elapsed = 0;

    if (s_state.timer_type == TIMER_ELAPSED) {
        return (int32_t)elapsed;
    } else {
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

    s_state.timer_type = TIMER_COUNTDOWN;
    s_state.timer_started_at = now_monotonic_sec();
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

    /* Revert to default mode, reset priority */
    s_state.mode = s_default_mode;
    s_state.priority = 2;

    notify_change();
    return ESP_OK;
}

esp_err_t state_set_auto_expire(int32_t duration_sec, status_mode_t revert_to)
{
    s_state.auto_expire_enabled = true;
    s_state.auto_expire_at = now_monotonic_sec() + duration_sec;
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
    int64_t now = now_monotonic_sec();

    /* Check pomodoro transitions */
    if (s_state.mode == MODE_POMODORO) {
        int32_t remaining = state_timer_get_seconds();

        if (remaining <= 0) {
            if (s_state.pomo_phase == POMO_WORK) {
                ESP_LOGI(TAG, "Pomodoro: work complete, starting break");
                s_state.pomo_phase = POMO_BREAK;
                s_state.timer_started_at = now;
                s_state.timer_duration_sec = s_state.pomo_break_sec;
                notify_change();
            } else if (s_state.pomo_phase == POMO_BREAK) {
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
        s_state.auto_expire_at = 0;
        s_state.mode = s_state.auto_expire_revert_to;
        s_state.priority = 2;
        s_state.timer_type = TIMER_NONE;
        s_state.timer_started_at = 0;
        s_state.timer_duration_sec = 0;
        s_state.subtitle[0] = '\0';
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
