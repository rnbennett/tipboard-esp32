idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board state ui network webserver mqtt_client lvgl esp_timer esp_psram
)
