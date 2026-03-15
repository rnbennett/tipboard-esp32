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
