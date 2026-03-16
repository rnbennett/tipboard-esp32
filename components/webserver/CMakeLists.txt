idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json state network board app_update log
    EMBED_FILES "${CMAKE_SOURCE_DIR}/data/index.html"
                "${CMAKE_SOURCE_DIR}/data/setup.html"
)
