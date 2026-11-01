# CMakeLists.txt
# ABOUTME: Build and upload targets for the OTA download spike (C1, #54).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike OtaDownload.
set(OTA_DOWNLOAD_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/OtaDownload")

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(OtaDownload
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-path ${OTA_DOWNLOAD_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling OtaDownload spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-OtaDownload
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${OTA_DOWNLOAD_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS OtaDownload
    COMMENT "Uploading OtaDownload spike"
    VERBATIM
)
