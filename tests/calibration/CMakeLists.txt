# CMakeLists.txt
# ABOUTME: Build target for the grayscale calibration test sketch.
# ABOUTME: Standalone Inkplate target with no network or sensor dependencies.

add_executable(GrayscaleCalibration
    GrayscaleCalibration.cpp
)
target_link_arduino_libraries(GrayscaleCalibration PRIVATE
    core
    Inkplate
)
target_enable_arduino_upload(GrayscaleCalibration)
