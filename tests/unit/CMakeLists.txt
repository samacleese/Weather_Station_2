# CMakeLists.txt
# ABOUTME: Standalone CMake project for host-side unit tests using GoogleTest.
# ABOUTME: Build with: cmake -S tests/unit -B build-host && cmake --build build-host
cmake_minimum_required(VERSION 3.16)
project(WeatherStationTests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/googletest
    ${CMAKE_CURRENT_BINARY_DIR}/googletest
    EXCLUDE_FROM_ALL
)

include(FetchContent)
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG        v6.21.5
)
FetchContent_MakeAvailable(ArduinoJson)

add_executable(unit_tests
    CACertsTest.cpp
    CurrentConditionsTest.cpp
    ../../src/security/CACerts.cpp
    ../../src/network/CurrentConditions.cpp
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_link_libraries(unit_tests PRIVATE gtest_main ArduinoJson)

include(GoogleTest)
gtest_discover_tests(unit_tests)
