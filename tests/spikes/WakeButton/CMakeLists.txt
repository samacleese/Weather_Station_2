# CMakeLists.txt
# ABOUTME: Build and upload targets for the wake button spike (C4 #57).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike WakeButton.
set(WAKE_BUTTON_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/WakeButton")

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(WakeButton
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-path ${WAKE_BUTTON_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling WakeButton spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-WakeButton
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${WAKE_BUTTON_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS WakeButton
    COMMENT "Uploading WakeButton spike"
    VERBATIM
)
