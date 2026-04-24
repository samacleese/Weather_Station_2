# ABOUTME: Reads layout.json and generates layout.h via configure_file.
# ABOUTME: Parallel to BoardOptions.cmake — include this after config.cmake in CMakeLists.txt.

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/layout.json")
file(READ "${CMAKE_SOURCE_DIR}/layout.json" LAYOUT_JSON)

# Main screen — temperature
string(JSON LAYOUT_MAIN_TEMPERATURE_X         GET ${LAYOUT_JSON} screens main temperature x)
string(JSON LAYOUT_MAIN_TEMPERATURE_Y         GET ${LAYOUT_JSON} screens main temperature y)
string(JSON LAYOUT_MAIN_TEMPERATURE_FONT_SIZE GET ${LAYOUT_JSON} screens main temperature font_size)

# Main screen — wind speed
string(JSON LAYOUT_MAIN_WIND_SPEED_X          GET ${LAYOUT_JSON} screens main wind_speed x)
string(JSON LAYOUT_MAIN_WIND_SPEED_Y          GET ${LAYOUT_JSON} screens main wind_speed y)
string(JSON LAYOUT_MAIN_WIND_SPEED_FONT_SIZE  GET ${LAYOUT_JSON} screens main wind_speed font_size)

# Main screen — description
string(JSON LAYOUT_MAIN_DESCRIPTION_X         GET ${LAYOUT_JSON} screens main description x)
string(JSON LAYOUT_MAIN_DESCRIPTION_Y         GET ${LAYOUT_JSON} screens main description y)
string(JSON LAYOUT_MAIN_DESCRIPTION_FONT_SIZE GET ${LAYOUT_JSON} screens main description font_size)

# Main screen — METAR
string(JSON LAYOUT_MAIN_METAR_X               GET ${LAYOUT_JSON} screens main metar x)
string(JSON LAYOUT_MAIN_METAR_Y               GET ${LAYOUT_JSON} screens main metar y)
string(JSON LAYOUT_MAIN_METAR_FONT_SIZE       GET ${LAYOUT_JSON} screens main metar font_size)

# Main screen — kitty position (dimensions come from Kitties::w/h)
string(JSON LAYOUT_MAIN_KITTY_X GET ${LAYOUT_JSON} screens main kitty x)
string(JSON LAYOUT_MAIN_KITTY_Y GET ${LAYOUT_JSON} screens main kitty y)

# Main screen — battery info
string(JSON LAYOUT_MAIN_BATTERY_INFO_X         GET ${LAYOUT_JSON} screens main battery_info x)
string(JSON LAYOUT_MAIN_BATTERY_INFO_Y         GET ${LAYOUT_JSON} screens main battery_info y)
string(JSON LAYOUT_MAIN_BATTERY_INFO_FONT_SIZE GET ${LAYOUT_JSON} screens main battery_info font_size)

# Main screen — warnings
string(JSON LAYOUT_MAIN_WARNINGS_X            GET ${LAYOUT_JSON} screens main warnings x)
string(JSON LAYOUT_MAIN_WARNINGS_Y            GET ${LAYOUT_JSON} screens main warnings y)
string(JSON LAYOUT_MAIN_WARNINGS_FONT_SIZE    GET ${LAYOUT_JSON} screens main warnings font_size)

# Loading screen — message
string(JSON LAYOUT_LOADING_MESSAGE_X         GET ${LAYOUT_JSON} screens loading message x)
string(JSON LAYOUT_LOADING_MESSAGE_Y         GET ${LAYOUT_JSON} screens loading message y)
string(JSON LAYOUT_LOADING_MESSAGE_FONT_SIZE GET ${LAYOUT_JSON} screens loading message font_size)

configure_file(src/layout.h.in ${CMAKE_BINARY_DIR}/generated/weather_station_2/layout.h)
