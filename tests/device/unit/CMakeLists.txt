# CMakeLists.txt
# ABOUTME: Build target for device unit tests using AUnit and the gtest adapter.
# ABOUTME: Flash and monitor serial output at 115200 baud for PASS/FAIL results.

file(COPY
    "${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/default.csv"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
)
file(RENAME
    "${CMAKE_CURRENT_BINARY_DIR}/default.csv"
    "${CMAKE_CURRENT_BINARY_DIR}/partitions.csv"
)

add_executable(DeviceUnitTests
    DeviceUnitTests.cpp
    ../../../src/display/Kitties.cpp
    ../../../src/display/KittyPics.cpp
    ../../../src/network/CurrentConditions.cpp
    ../../../src/network/Network.cpp
    ../../../src/security/CACerts.cpp
)

target_include_directories(DeviceUnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

set(ARDUINO_LIB_AUnit_PATH "/root/Arduino/libraries/AUnit" CACHE PATH "AUnit library path")

target_link_arduino_libraries(DeviceUnitTests PRIVATE
    core
    AUnit
    Inkplate
    ArduinoJson
    ArduinoLog
    LCBUrl
)

target_enable_arduino_upload(DeviceUnitTests)
