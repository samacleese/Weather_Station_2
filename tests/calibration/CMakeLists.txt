# CMakeLists.txt
# ABOUTME: Build target for the grayscale calibration test sketch.
# ABOUTME: Standalone Inkplate target with no network or sensor dependencies.
set(CALIBRATION_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/GrayscaleCalibration")

add_custom_target(GrayscaleCalibration ALL
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "PSRAM=enabled,PartitionScheme=default,CPUFreq=240"
        --build-path ${CALIBRATION_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling GrayscaleCalibration via arduino-cli"
    VERBATIM
)

add_custom_target(upload-GrayscaleCalibration
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${CALIBRATION_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS GrayscaleCalibration
    COMMENT "Uploading GrayscaleCalibration"
    VERBATIM
)
