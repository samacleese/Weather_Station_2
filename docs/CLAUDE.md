# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Weather Station 2 is an Arduino project for the Inkplate 10 E-paper display that shows current weather conditions from weather.gov alongside rotating cat images. The device operates on a deep sleep cycle, waking every 10 minutes (600 seconds) to fetch and display updated weather data.

## Hardware & Platform

- **Target Hardware**: Inkplate 10 E-paper Display (ESP32-based)
- **Arduino Board**: Must be configured for "Inkplate 10" in Arduino IDE
- **Libraries Required**: Inkplate library, ArduinoLog, ArduinoJson, standard ESP32 libraries

## Project Structure

```
Weather_Station_2/
├── Weather_Station_2.ino          # Main Arduino sketch
├── src/                           # Core application code
│   ├── network/                   # Network and API components
│   │   ├── Network.h/.cpp         # WiFi management
│   │   └── CurrentConditions.h/.cpp # Weather.gov API client
│   ├── display/                   # Display and UI components
│   │   ├── DisplayLocations.h/.cpp # Text layout and positioning
│   │   ├── Kitties.h/.cpp         # Cat image rotation logic
│   │   └── KittyPics.h/.cpp       # Cat image bitmap data
│   └── security/                  # SSL and certificate management
│       └── CACerts.h/.cpp         # Certificate store
├── assets/                        # Non-code resources
│   ├── fonts/                     # Roboto font family headers
│   ├── images/                    # Source PNG images
│   └── certificates/              # SSL certificate files
├── tools/                         # Development utilities
│   ├── image_converter.py         # PNG to bitmap converter
│   └── cert_collector.py          # SSL certificate updater
└── docs/                          # Documentation
    ├── README.md
    └── CLAUDE.md
```

## Code Architecture

### Core Components

- **Weather_Station_2.ino**: Main application file with setup() and empty loop() (uses deep sleep)
- **src/network/CurrentConditions**: Handles weather.gov API communication with robust error handling and retry logic
- **src/network/Network**: WiFi management with connection recovery and timeout handling
- **src/display/Kitties**: Manages rotating cat image display using pre-converted bitmap arrays
- **src/security/CACerts**: SSL certificate management for secure HTTPS connections

### Key Design Patterns

- **Smart Pointers**: Uses custom `std::make_unique` implementation for ESP32 compatibility
- **Error Resilience**: Comprehensive error codes and retry mechanisms throughout network stack
- **Deep Sleep Cycle**: Device sleeps 10 minutes normally, 5 minutes on errors to conserve battery
- **Fallback Data**: Displays partial data when available, shows error messages when not

### Network & SSL

- **Certificate Management**: `tools/cert_collector.py` script generates CACerts.h from domain certificates
- **Weather API**: Uses weather.gov API with station identifier (default: KBFI)
- **Retry Logic**: Built-in retry mechanisms for network failures and API timeouts

### Display System

- **Fonts**: Roboto family fonts in multiple sizes stored as header files in `assets/fonts/`
- **Layout**: Weather data on left, cat images on right, system info at bottom
- **Image Format**: Cat images converted from PNG to 3-bit bitmap arrays via `tools/image_converter.py`

## Development Commands

This is an Arduino project. Development can use Arduino IDE or arduino-cli:

- **Compile**: `arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10`
- **Upload**: Use Arduino IDE "Upload" button or arduino-cli upload
- **Code Formatting**: `clang-format -i src/**/*.cpp src/**/*.h` (Google style with 120 char limit)

## Utility Scripts

- **Certificate Updates**: `python3 tools/cert_collector.py -u src/security/CACerts.h -f src/security/CACerts.h`
- **Image Conversion**: `python3 tools/image_converter.py` (converts PNG files from assets/images/ to C++ bitmap arrays)

## Network Configuration

WiFi credentials are hardcoded in Weather_Station_2.ino setup() function:
```cpp
auto network = std::make_shared<Network>("SSID", "PASSWORD");
```

## Error Handling Strategy

The codebase implements comprehensive error handling:
- Network errors trigger shorter sleep cycles for faster recovery
- Partial weather data is displayed when available
- User-friendly error messages show on display
- System continues operating even with API failures