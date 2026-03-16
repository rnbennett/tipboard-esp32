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
    int64_t timer_started_at;     /* monotonic seconds when timer started, 0 if none */
    int32_t timer_duration_sec;   /* countdown duration, 0 for elapsed */

    /* Auto-expire */
    bool auto_expire_enabled;
    int64_t auto_expire_at;       /* monotonic seconds, 0 if disabled */
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

/* Called periodically (e.g., every 1s from LVGL timer) to check auto-expire and pomodoro transitions */
void state_tick(void);

/* Default mode configuration */
esp_err_t state_set_default_mode(status_mode_t mode);
status_mode_t state_get_default_mode(void);

/* ── Device configuration (persisted to LittleFS) ── */

typedef struct {
    char mode_labels[MODE_COUNT][24];     /* Custom display labels per mode */
    char mode_subtitles[MODE_COUNT][64];  /* Default subtitles per mode */
    status_mode_t default_mode;
    int32_t pomo_work_min;
    int32_t pomo_break_min;
    uint8_t brightness;
    uint8_t dim_brightness;
    uint8_t dim_start_hour;
    uint8_t dim_end_hour;
    char timezone[48];                    /* POSIX TZ string e.g. "EST5EDT,M3.2.0,M11.1.0" */
    char weather_lat[16];                 /* Latitude e.g. "0.0000" */
    char weather_lon[16];                 /* Longitude e.g. "0.0000" */
    char mqtt_broker[64];                 /* MQTT broker URI e.g. "mqtt://YOUR_MQTT_BROKER_IP:1883" */
} device_config_t;

/* Get pointer to device config */
const device_config_t *config_get(void);

/* Update device config (saves to LittleFS) */
esp_err_t config_set(const device_config_t *cfg);

/* Initialize config (load from LittleFS or defaults) */
esp_err_t config_init(void);
