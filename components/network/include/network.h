#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,
} wifi_state_t;

/* Initialize WiFi subsystem (NVS, netif, event loop). Call once at boot. */
esp_err_t network_init(void);

/* Connect using NVS credentials. Starts AP mode if none stored. Non-blocking. */
esp_err_t network_wifi_connect(void);

/* Store WiFi credentials to NVS */
esp_err_t network_set_credentials(const char *ssid, const char *password);

/* Query state */
wifi_state_t network_get_state(void);
const char *network_get_ip(void);
const char *network_get_ssid(void);
