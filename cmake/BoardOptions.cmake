# ABOUTME: Board identity and options for the Inkplate 10, passed to arduino-cli compile/upload.
# ABOUTME: Single source of truth for FQBN and the board-options shared by all three CMake targets.
# PartitionScheme legitimately differs per target (WeatherStation needs huge_app; the test
# sketches use default) and is composed onto INKPLATE10_COMMON_BOARD_OPTIONS by each target.
#
# FQBN is Soldered's own board package (soldered-inkplate-boards), not mainline esp32:esp32.
# "Inkplate10" (build.board=INKPLATE10, labeled "e-radionica Inkplate 10" in arduino-cli) is the
# original Inkplate 10 hardware revision -- do not confuse with "Inkplate10V2"
# (build.board=INKPLATE10V2, labeled "Soldered Inkplate 10"), a newer PCB revision with a
# different pinout that this project's hardware is not.
set(INKPLATE10_FQBN "soldered-inkplate-boards:esp32:Inkplate10")
set(INKPLATE10_COMMON_BOARD_OPTIONS "PSRAM=enabled,CPUFreq=240")
