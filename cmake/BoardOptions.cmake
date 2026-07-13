# ABOUTME: Board identity and options for the Inkplate 10, passed to arduino-cli compile/upload.
# ABOUTME: Single source of truth for FQBN + board-options — see cmake/inkplate10-board.txt for the board definition itself.
set(INKPLATE10_FQBN "esp32:esp32:Inkplate10")
set(INKPLATE10_BOARD_OPTIONS "PSRAM=enabled,PartitionScheme=huge_app,CPUFreq=240")
