# ABOUTME: Container build environment for Weather Station firmware.
# ABOUTME: Pins arduino-cli, board support, and all library versions for reproducible builds.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    python3 \
    python3-pip \
    python-is-python3 \
    cmake \
    ninja-build \
    git \
    clang-format \
    build-essential \
    libgtest-dev \
    libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# Pin arduino-cli version by downloading the release binary directly
RUN curl -fsSL \
    "https://github.com/arduino/arduino-cli/releases/download/v1.3.1/arduino-cli_1.3.1_Linux_64bit.tar.gz" \
    | tar -xz -C /usr/local/bin arduino-cli

# Stub Arduino IDE installation (lib/version.txt). Left over from the removed
# Arduino-CMake-Toolchain, which checked for this file. Whether arduino-cli itself still
# needs it hasn't been re-verified since the toolchain's removal -- don't assume either way.
RUN mkdir -p /opt/arduino/lib && echo "1.8.16" > /opt/arduino/lib/version.txt

# Stub preferences.txt setting sketchbook.path=/root/Arduino. Left over from the removed
# Arduino-CMake-Toolchain, which read this file to discover the sketchbook path (and thus
# ~/Arduino/libraries where arduino-cli installs user libraries). Whether arduino-cli itself
# still needs it hasn't been re-verified since the toolchain's removal -- don't assume either
# way.
RUN mkdir -p /root/.arduino15 \
    && echo "sketchbook.path=/root/Arduino" > /root/.arduino15/preferences.txt

# The board platform bundles a large toolchain (500+MB across several tool archives);
# the default network timeout is too short for it on a slow connection.
RUN arduino-cli config init \
    && arduino-cli config set network.connection_timeout 600s

# Pin board support package. Uses Soldered's own board package rather than mainline
# esp32:esp32: Croduino_Boards:Inkplate (the old package) is unmaintained (no release since
# 2022) and only ever bundled ESP32 Arduino 1.0.5-rc2, too old for current InkplateLibrary
# releases, but Soldered's current package (unlike Croduino_Boards) tracks a recent IDF release
# and ships a native Inkplate 10 board, so no custom board file is needed on top of it.
# "Inkplate10" (not "Inkplate10V2", a newer PCB revision with a different pinout) matches this
# project's hardware -- see cmake/BoardOptions.cmake.
RUN arduino-cli config set board_manager.additional_urls \
        "https://github.com/SolderedElectronics/Inkplate-Board-Definitions-for-Arduino-IDE/raw/refs/heads/main/package_Inkplate_Boards_index.json" \
    && arduino-cli core update-index \
    && arduino-cli core install soldered-inkplate-boards:esp32@3.0.0

# Pin libraries.
RUN arduino-cli lib install \
    "ArduinoJson@6.18.5" \
    "ArduinoLog@1.1.1" \
    "LCBUrl@1.1.4" \
    "AUnit@1.7.1" \
    "InkplateLibrary@11.1.2"

# Python tooling: image codegen (tools/image_converter.py) and esptool (board package).
# --break-system-packages is required on Ubuntu 24.04, which ships with PEP 668
# EXTERNALLY-MANAGED protection even inside a container.
# pyserial is required by esptool.py (bundled in the board package) to generate the .bin from
# the compiled .elf.
RUN pip3 install --no-cache-dir --break-system-packages Pillow numpy pyserial
