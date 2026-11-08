# ABOUTME: Build and upload targets for the FirmwareInstaller hardware spike (D2 #59).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike OtaInstall.
# Like OtaCheck, src/ here mirrors the real src/ with symlinks, because arduino-cli only compiles .cpp
# files inside the sketch directory.
set(OTA_INSTALL_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/OtaInstall")

# The stalled-server case needs stall_server.py (next to this file) running on the dev machine. Set
# OTA_STALL_URL in config.cmake to its URL, e.g. http://192.168.1.50:8000/firmware. Empty fails that case.
if(NOT DEFINED OTA_STALL_URL)
    set(OTA_STALL_URL "")
endif()
set(OTA_INSTALL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(OtaInstallSettings.h.in ${OTA_INSTALL_GENERATED_DIR}/OtaInstallSettings.h)

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(OtaInstall
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR} -I${OTA_INSTALL_GENERATED_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR} -I${OTA_INSTALL_GENERATED_DIR}"
        --build-path ${OTA_INSTALL_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling OtaInstall spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-OtaInstall
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${OTA_INSTALL_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS OtaInstall
    COMMENT "Uploading OtaInstall spike"
    VERBATIM
)
