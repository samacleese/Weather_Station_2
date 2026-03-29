# CMakeLists.txt
# ABOUTME: Root CMake build file for the Weather Station firmware.
# ABOUTME: Uses arduino-cmake-toolchain to target the Inkplate 10 (ESP32).
cmake_minimum_required(VERSION 3.16)
project(WeatherStation CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Firmware target ───────────────────────────────────────────────────────────
add_executable(WeatherStation
    Weather_Station_2.cpp
    src/display/DisplayLocations.cpp
    src/display/Kitties.cpp
    src/display/KittyPics.cpp
    src/network/CurrentConditions.cpp
    src/network/Network.cpp
    src/security/CACerts.cpp
)

target_include_directories(WeatherStation PRIVATE src)

# ── Arduino libraries ─────────────────────────────────────────────────────────
target_link_arduino_libraries(WeatherStation PRIVATE
    core
    Inkplate
    ArduinoJson
    ArduinoLog
    LCBUrl
)

target_enable_arduino_upload(WeatherStation)

# ── Partition table (huge_app) ────────────────────────────────────────────────
# file(COPY ... RENAME) is not available in CMake 3.16; use two-step copy+rename.
file(COPY
    "${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/huge_app.csv"
    DESTINATION "${CMAKE_BINARY_DIR}"
)
file(RENAME
    "${CMAKE_BINARY_DIR}/huge_app.csv"
    "${CMAKE_BINARY_DIR}/partitions.csv"
)
