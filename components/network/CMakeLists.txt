idf_component_register(
    SRCS "network.c" "ntp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip log
)
