idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board state ui network webserver lvgl esp_timer esp_psram
)
