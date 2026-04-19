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

### Build, flash, and test

Copy `config.cmake.example` to `config.cmake` and fill in your WiFi credentials and station ID first.

`build.sh` at the repo root wraps all container operations and auto-detects podman vs docker:

```bash
./build.sh configure           # cmake configure (once, or when CMakeLists.txt changes)
./build.sh build               # compile firmware
./build.sh flash               # flash to /dev/ttyUSB0
./build.sh monitor             # open serial monitor
./build.sh test-host           # build and run host unit tests
./build.sh build-device-tests  # compile device test sketch
./build.sh flash-device-tests  # flash device tests
```

The device appears on `/dev/ttyUSB0`. Your account must be in the `dialout` group on the host. Override the port if needed:

```bash
SERIAL_PORT=/dev/ttyACM0 ./build.sh flash
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
