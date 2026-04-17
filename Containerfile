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

# Pin board support package
RUN arduino-cli config add board_manager.additional_urls \
        https://github.com/SolderedElectronics/Croduino-Board-Definitions-for-Arduino-IDE/raw/master/package_Croduino_Boards_index.json \
    && arduino-cli core update-index \
    && arduino-cli core install Croduino_Boards:Inkplate@1.0.1

# Pin libraries
RUN arduino-cli lib install \
    "ArduinoJson@6.18.5" \
    "ArduinoLog@1.1.1" \
    "InkplateLibrary@10.2.2" \
    "LCBUrl@1.1.4"

# InkplateLibrary 10.2.2 calls WiFiClientSecure::setInsecure(), which does not exist in
# the ESP32 Arduino 1.0.5-rc2 core bundled with Croduino_Boards:Inkplate@1.0.1.
# setInsecure() was added in ESP32 Arduino 1.0.6; this patch keeps compilation working
# against the pinned board core.
RUN sed -i 's/^\( *\)client->setInsecure(); \/\/ Use HTTPS/\1\/\/ client->setInsecure(); \/\/ Use HTTPS/' \
    /root/Arduino/libraries/InkplateLibrary/src/include/NetworkClient.cpp

# Python tooling: image codegen (tools/image_converter.py) and esptool (board package).
# --break-system-packages is required on Ubuntu 24.04, which ships with PEP 668
# EXTERNALLY-MANAGED protection even inside a container.
# pyserial is required by esptool.py (bundled in Croduino_Boards:Inkplate@1.0.1) to
# generate the .bin from the compiled .elf.
RUN pip3 install --no-cache-dir --break-system-packages Pillow numpy pyserial
