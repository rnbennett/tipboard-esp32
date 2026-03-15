#pragma once

#include "esp_err.h"

/* Start the HTTP server (call after WiFi is connected) */
esp_err_t webserver_start(void);

/* Stop the HTTP server */
esp_err_t webserver_stop(void);

/* Push state update to all connected WebSocket clients */
void webserver_notify_clients(void);
