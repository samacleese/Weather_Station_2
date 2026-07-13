# CMakeLists.txt
# ABOUTME: Build target for device unit tests using AUnit and the gtest adapter.
# ABOUTME: Flash and monitor serial output at 115200 baud for PASS/FAIL results.
set(DEVICE_TESTS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/DeviceUnitTests")

add_custom_target(DeviceUnitTests
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "PSRAM=enabled,PartitionScheme=default,CPUFreq=240"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-path ${DEVICE_TESTS_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling DeviceUnitTests via arduino-cli"
    VERBATIM
)

add_custom_target(upload-DeviceUnitTests
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${DEVICE_TESTS_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS DeviceUnitTests
    COMMENT "Uploading DeviceUnitTests"
    VERBATIM
)
