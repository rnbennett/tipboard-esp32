idf_component_register(
    SRCS "network.c" "ntp.c" "weather.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip log esp_http_client esp-tls json state
)
