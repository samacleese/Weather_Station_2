# ABOUTME: Board identity and options for the Inkplate 10, passed to arduino-cli compile/upload.
# ABOUTME: Single source of truth for FQBN and the board-options shared by all three CMake targets —
# ABOUTME: see cmake/inkplate10-board.txt for the board definition itself.
# PartitionScheme legitimately differs per target (WeatherStation needs huge_app; the test
# sketches use default) and is composed onto INKPLATE10_COMMON_BOARD_OPTIONS by each target.
set(INKPLATE10_FQBN "esp32:esp32:Inkplate10")
set(INKPLATE10_COMMON_BOARD_OPTIONS "PSRAM=enabled,CPUFreq=240")
