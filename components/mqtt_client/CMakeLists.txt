idf_component_register(
    SRCS "tipboard_mqtt.c" "cal_format.c"
    INCLUDE_DIRS "include" "."
    REQUIRES mqtt state json log
)
