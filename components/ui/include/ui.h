#pragma once

#include "esp_err.h"
#include "state.h"
#include "lvgl.h"

/* Initialize the UI system. Call after lv_init() and display driver setup. */
esp_err_t ui_init(void);

/* Update UI to reflect current state. Safe to call from LVGL task only. */
void ui_update(const status_state_t *state);

/* Update just the timer display (called every second from LVGL timer) */
void ui_update_timer(int32_t seconds, timer_type_t type);

/* Update top bar time display */
void ui_update_time(const char *time_str, const char *date_str);

/* Update WiFi status in top bar */
void ui_update_wifi_status(const char *ip, bool connected);

/* Get the main screen object */
lv_obj_t *ui_get_screen(void);
