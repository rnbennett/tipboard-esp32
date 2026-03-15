#pragma once

#include "esp_err.h"

/* Start MQTT client. Call after WiFi is connected. */
esp_err_t tipboard_mqtt_init(void);

/* Publish current state to tipboard/status. Called on state changes. */
void tipboard_mqtt_publish_state(void);
