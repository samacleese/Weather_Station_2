# CMakeLists.txt
# ABOUTME: Build and upload targets for the OTA manifest check spike (D1 #58).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike OtaCheck.
# Unlike the Track C spikes, this one exercises the real OTAManager and WeatherNetwork. arduino-cli only
# compiles .cpp files inside the sketch directory, so src/ here mirrors the real src/ with symlinks, the
# same way tests/device/DeviceUnitTests does.
set(OTA_CHECK_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/OtaCheck")

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(OtaCheck
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-path ${OTA_CHECK_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling OtaCheck spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-OtaCheck
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${OTA_CHECK_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS OtaCheck
    COMMENT "Uploading OtaCheck spike"
    VERBATIM
)
