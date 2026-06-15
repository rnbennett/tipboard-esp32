cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Load .env file if it exists — provides build-time defaults
set(ENV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.env")
if(EXISTS "${ENV_FILE}")
    file(STRINGS "${ENV_FILE}" ENV_LINES)
    foreach(LINE ${ENV_LINES})
        if(LINE MATCHES "^([A-Za-z_]+)=(.*)$")
            set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
            message(STATUS ".env: ${CMAKE_MATCH_1} = [set]")
        endif()
    endforeach()
endif()

# Board selection — env var overrides .env file
if(DEFINED ENV{TIPBOARD_BOARD})
    set(TIPBOARD_BOARD "$ENV{TIPBOARD_BOARD}")
endif()
if(NOT DEFINED TIPBOARD_BOARD)
    set(TIPBOARD_BOARD "p4")
endif()
message(STATUS "Board: ${TIPBOARD_BOARD}")

if(TIPBOARD_BOARD STREQUAL "cyd")
    set(BOARD_DEFINE "BOARD_CYD")
elseif(TIPBOARD_BOARD STREQUAL "p4")
    set(BOARD_DEFINE "BOARD_P4")
else()
    message(FATAL_ERROR "Unknown TIPBOARD_BOARD: ${TIPBOARD_BOARD}. Use 'p4' or 'cyd'.")
endif()

# Set defaults if not provided
if(NOT DEFINED TIPBOARD_TIMEZONE)
    set(TIPBOARD_TIMEZONE "UTC0")
endif()
if(NOT DEFINED TIPBOARD_WEATHER_LAT)
    set(TIPBOARD_WEATHER_LAT "0")
endif()
if(NOT DEFINED TIPBOARD_WEATHER_LON)
    set(TIPBOARD_WEATHER_LON "0")
endif()
if(NOT DEFINED TIPBOARD_MQTT_BROKER)
    set(TIPBOARD_MQTT_BROKER "")
endif()

# Build compile flags string.
# NOTE: WiFi credentials are deliberately NOT compiled in — baking the SSID/pass
# into the firmware binary made them recoverable with `esptool read_flash` +
# `strings`. Credentials live only in NVS now, provisioned via the captive-portal
# setup page (or `network_set_credentials`). Non-secret defaults stay as -D flags.
add_compile_options(
    "-D${BOARD_DEFINE}"
    "-DTIPBOARD_TIMEZONE=\"${TIPBOARD_TIMEZONE}\""
    "-DTIPBOARD_WEATHER_LAT=\"${TIPBOARD_WEATHER_LAT}\""
    "-DTIPBOARD_WEATHER_LON=\"${TIPBOARD_WEATHER_LON}\""
    "-DTIPBOARD_MQTT_BROKER=\"${TIPBOARD_MQTT_BROKER}\""
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tipboard)
