# ABOUTME: Runs git describe and generates version.h via configure_file.
# ABOUTME: Parallel to Layout.cmake — include this after config.cmake in CMakeLists.txt.

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/HEAD")

file(READ "${CMAKE_SOURCE_DIR}/VERSION" FIRMWARE_VERSION)
string(STRIP "${FIRMWARE_VERSION}" FIRMWARE_VERSION)

execute_process(
    COMMAND git describe --always --dirty --abbrev=7
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_DESCRIBE_RESULT
    ERROR_QUIET
)

if(NOT GIT_DESCRIBE_RESULT EQUAL 0 OR NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()

configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/weather_station_2/version.h)
