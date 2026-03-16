idf_component_register(
    SRCS "state.c" "persist.c" "config.c"
    INCLUDE_DIRS "include"
    REQUIRES json esp_timer
    PRIV_REQUIRES littlefs
)
