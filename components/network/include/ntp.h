#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/* Start NTP sync. Call after WiFi is connected. */
esp_err_t ntp_init(void);

/* Check if time has been synced at least once */
bool ntp_is_synced(void);

/* Get formatted time string (e.g., "2:45 PM") */
void ntp_get_time_str(char *buf, size_t len);

/* Get formatted date string (e.g., "Sat, Mar 15") */
void ntp_get_date_str(char *buf, size_t len);
