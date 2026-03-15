#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    float temp_f;               /* Temperature in Fahrenheit */
    int weather_code;           /* WMO weather code */
    int humidity;               /* Relative humidity % */
    int precip_chance;          /* Precipitation probability % */
    bool valid;                 /* Data has been fetched at least once */
} weather_data_t;

/* Start weather polling task (fetches every 15 min). Call after WiFi connected. */
esp_err_t weather_init(void);

/* Get current weather data (may be stale if fetch failed) */
const weather_data_t *weather_get(void);

/* Get short description from WMO weather code (e.g., "Sunny", "Rain") */
const char *weather_code_desc(int code);

/* Get condition icon character for top bar display */
const char *weather_code_icon(int code);
