# CMakeLists.txt
# ABOUTME: Build target for the grayscale calibration test sketch.
# ABOUTME: Standalone Inkplate target with no network or sensor dependencies.

# Partition table — toolchain expects partitions.csv in the target's binary dir.
# The calibration sketch is small; the default partition scheme is sufficient.
file(COPY
    "${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/default.csv"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
)
file(RENAME
    "${CMAKE_CURRENT_BINARY_DIR}/default.csv"
    "${CMAKE_CURRENT_BINARY_DIR}/partitions.csv"
)

add_executable(GrayscaleCalibration
    GrayscaleCalibration.cpp
)
target_link_arduino_libraries(GrayscaleCalibration PRIVATE
    core
    Inkplate
)
target_enable_arduino_upload(GrayscaleCalibration)
