idf_component_register(
    SRCS "tipboard_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt state json log
)
