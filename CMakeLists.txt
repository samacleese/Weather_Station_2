# CMakeLists.txt
# ABOUTME: Root CMake build file for the Weather Station firmware.
# ABOUTME: Drives arduino-cli compile/upload as custom targets; CMake itself only handles codegen.
cmake_minimum_required(VERSION 3.16)
project(WeatherStation NONE)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.cmake")
    message(FATAL_ERROR "config.cmake not found. Copy config.cmake.example to config.cmake and fill in your values.")
endif()
include("${CMAKE_SOURCE_DIR}/config.cmake")

# libraries/InkplateLibrary is a git submodule; a plain clone/pull leaves it registered but
# empty until explicitly initialized. Do that automatically so a fresh checkout builds without
# needing to know about the submodule ahead of time.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/libraries/InkplateLibrary/library.properties")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- libraries/InkplateLibrary
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMODULE_RESULT
    )
    if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to checkout libraries/InkplateLibrary submodule.")
    endif()
endif()
include(cmake/Layout.cmake)
include(cmake/Version.cmake)
include(cmake/BoardOptions.cmake)

set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR}/weather_station_2)
configure_file(src/user_settings.h.in ${GENERATED_INCLUDE_DIR}/weather_station_2/user_settings.h)

# ── Firmware target ───────────────────────────────────────────────────────────
set(WEATHER_STATION_BUILD_DIR "${CMAKE_BINARY_DIR}/WeatherStation")

add_custom_target(WeatherStation ALL
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=huge_app"
        --library ${CMAKE_SOURCE_DIR}/libraries/InkplateLibrary
        --build-property "compiler.cpp.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-path ${WEATHER_STATION_BUILD_DIR}
        ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling WeatherStation firmware via arduino-cli"
    VERBATIM
)

add_custom_target(upload-WeatherStation
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${WEATHER_STATION_BUILD_DIR} ${CMAKE_SOURCE_DIR}"
    DEPENDS WeatherStation
    COMMENT "Uploading WeatherStation firmware"
    VERBATIM
)

# ── Test sketches ─────────────────────────────────────────────────────────────
add_subdirectory(tests)
