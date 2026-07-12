# ABOUTME: Pre-selected board configuration for Inkplate 10 (ESP32).
# ABOUTME: Passed to CMake via -DARDUINO_BOARD_OPTIONS_FILE to avoid manual board selection.
#
# Board identifier is "<esp32:esp32 architecture>.<board prefix>" per Arduino-CMake-Toolchain's
# BoardsIndex.cmake, i.e. "esp32.Inkplate10" for the custom board defined in
# cmake/inkplate10-board.txt (appended onto the esp32:esp32 package's boards.txt).
set(ARDUINO_BOARD "Inkplate 10 (ESP32, esp32:esp32 core) [esp32.Inkplate10]")
set(ARDUINO_ESP32_INKPLATE10_MENU_PSRAM_ENABLED TRUE)
set(ARDUINO_ESP32_INKPLATE10_MENU_PARTITIONSCHEME_HUGE_APP TRUE)
set(ARDUINO_ESP32_INKPLATE10_MENU_CPUFREQ_240 TRUE)
