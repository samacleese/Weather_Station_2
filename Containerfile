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

# Stub Arduino IDE installation required by Arduino-CMake-Toolchain to locate packages.
# The toolchain only checks for lib/version.txt; all actual tooling comes from ~/.arduino15.
RUN mkdir -p /opt/arduino/lib && echo "1.8.16" > /opt/arduino/lib/version.txt

# Stub preferences.txt so Arduino-CMake-Toolchain discovers the sketchbook path
# (and thus ~/Arduino/libraries where arduino-cli installs user libraries).
RUN mkdir -p /root/.arduino15 \
    && echo "sketchbook.path=/root/Arduino" > /root/.arduino15/preferences.txt

# Pin Arduino-CMake-Toolchain at the commit previously tracked as a submodule.
# This repo has no release tags, so a commit hash is the only stable pin.
# --no-tags suppresses tag fetching for a leaner clone.
RUN git clone --no-tags https://github.com/a9183756-gh/Arduino-CMake-Toolchain.git \
        /opt/arduino-cmake-toolchain \
    && cd /opt/arduino-cmake-toolchain \
    && git checkout e745a9bed3c3fb83442d55bf05630f31574674f2

# Pin board support package. esp32:esp32 (Espressif's mainline core) is listed in
# arduino-cli's default package index, so no board_manager.additional_urls entry is needed.
# Croduino_Boards:Inkplate was dropped: its board package repo is unmaintained (no release
# since 2022) and only ever bundled ESP32 Arduino 1.0.5-rc2, too old for current
# InkplateLibrary releases (see cmake/inkplate10-board.txt for the custom board this adds).
RUN arduino-cli core update-index \
    && arduino-cli core install esp32:esp32@3.3.10

# Register the Inkplate 10 as a board under esp32:esp32 (see cmake/inkplate10-board.txt for
# what this defines and why).
COPY cmake/inkplate10-board.txt /tmp/inkplate10-board.txt
RUN cat /tmp/inkplate10-board.txt \
    >> /root/.arduino15/packages/esp32/hardware/esp32/3.3.10/boards.txt \
    && rm /tmp/inkplate10-board.txt

# Pin libraries
RUN arduino-cli lib install \
    "ArduinoJson@6.18.5" \
    "ArduinoLog@1.1.1" \
    "InkplateLibrary@10.2.2" \
    "LCBUrl@1.1.4" \
    "AUnit@1.7.1"

# Python tooling: image codegen (tools/image_converter.py) and esptool (board package).
# --break-system-packages is required on Ubuntu 24.04, which ships with PEP 668
# EXTERNALLY-MANAGED protection even inside a container.
# pyserial is required by esptool.py (bundled in esp32:esp32) to generate the .bin from the
# compiled .elf.
RUN pip3 install --no-cache-dir --break-system-packages Pillow numpy pyserial
