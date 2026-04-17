# Weather Station 2

A delightful weather station application for the Inkplate 10 display that shows current weather conditions along with a random cat image to brighten your day!

## Overview

This project turns your Inkplate 10 into a weather station that displays:
- Current weather conditions from weather.gov
- A random cat image from a pre-loaded set of images
- Weather information updates periodically

## Hardware Requirements

- Inkplate 10 E-paper Display
- Power supply for the Inkplate
- WiFi connection for weather data updates

## Software Dependencies

- Arduino IDE
- Inkplate 10 library
- Required Arduino libraries for weather.gov API interaction

## Installation

1. Install the Arduino IDE
2. Install the Inkplate 10 library through the Arduino Library Manager
3. Clone or download this repository
4. Open the project in Arduino IDE
5. Upload the code to your Inkplate 10 device

## Container build

A `Containerfile` at the repo root pins all toolchain dependencies for a reproducible build without requiring a local Arduino IDE installation.

### Build the image

```bash
podman build -t weather-station-builder .
# or: docker build -t weather-station-builder .
```

### Compile firmware

Copy `config.cmake.example` to `config.cmake` and fill in your WiFi credentials and station ID first.

```bash
# Podman (Linux with SELinux — the :Z flag relabels the volume for container access)
podman run --rm -v $(pwd):/project:Z -w /project weather-station-builder \
    bash -c "cmake \
        -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
        -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
        -B build -G Ninja \
    && cmake --build build"

# Docker (or Podman without SELinux)
docker run --rm -v $(pwd):/project -w /project weather-station-builder \
    bash -c "cmake \
        -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
        -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
        -B build -G Ninja \
    && cmake --build build"
```

### Flash firmware

The device appears on `/dev/ttyUSB0`. Your account must be in the `dialout` group on the host.

```bash
# Podman — --group-add keep-groups passes your host supplementary groups (including
# dialout) into the container, required for rootless Podman to access the serial port.
podman run --rm -v $(pwd):/project:Z --device /dev/ttyUSB0 --group-add keep-groups \
    -w /project weather-station-builder \
    bash -c "SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation"

# Docker
docker run --rm -v $(pwd):/project --device /dev/ttyUSB0 -w /project \
    weather-station-builder \
    bash -c "SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation"
```

## Features

- Real-time weather data from weather.gov
- Display of current weather conditions
- Random cat images for entertainment
- E-paper display for clear visibility and low power consumption

## Contributing

Contributions to improve the Weather Station are welcome! Feel free to:
- Submit bug reports
- Propose new features
- Submit pull requests

## License

[Include your preferred license information here]

## Acknowledgments

- Weather data provided by weather.gov
- Built for the Inkplate 10 platform
