idf_component_register(
    SRCS "board.c" "boards/cyd/board_cyd.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_lcd_touch esp_hw_support
)
