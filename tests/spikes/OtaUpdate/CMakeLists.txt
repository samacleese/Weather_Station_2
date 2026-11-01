# CMakeLists.txt
# ABOUTME: Build and upload targets for the OTA update spike (C1 #54 download, C2 #55 signature, C3 #56 write).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike OtaUpdate.
set(OTA_UPDATE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/OtaUpdate")

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(OtaUpdate
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-path ${OTA_UPDATE_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling OtaUpdate spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-OtaUpdate
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${OTA_UPDATE_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS OtaUpdate
    COMMENT "Uploading OtaUpdate spike"
    VERBATIM
)
