idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json state network board log
    EMBED_FILES "${CMAKE_SOURCE_DIR}/data/index.html"
)
