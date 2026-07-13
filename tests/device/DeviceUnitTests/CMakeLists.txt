# CMakeLists.txt
# ABOUTME: Build target for device unit tests using AUnit and the gtest adapter.
# ABOUTME: Flash and monitor serial output at 115200 baud for PASS/FAIL results.
#
# The -I flags below only make src/ headers visible; arduino-cli's sketch convention
# only auto-compiles .cpp files under the sketch's own directory or its local src/
# subfolder. DeviceUnitTests.ino calls into Kitties, KittyPics, CurrentConditions,
# Network, and CACerts, so tests/device/DeviceUnitTests/src/ mirrors the real src/
# subtree with symlinks (not copies) to those five .cpp files and their headers, so
# arduino-cli picks them up and they stay in sync with the real files.
set(DEVICE_TESTS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/DeviceUnitTests")

add_custom_target(DeviceUnitTests
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=default"
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
