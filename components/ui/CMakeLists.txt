idf_component_register(
    SRCS "ui_theme.c" "ui_screen.c" "ui_geodesic.c" "ui_timer_arc.c" "ui_transitions.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl state
)
