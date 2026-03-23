# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Weather Station 2 is an Arduino/ESP32 project for the Inkplate 10 E-paper display. It fetches weather data from weather.gov and displays it alongside rotating cat images. The device wakes every 10 minutes via deep sleep to conserve battery.

## Build, Flash & Serial

The device appears on `/dev/ttyUSB0`. The user must be in the `dialout` group (already configured). `arduino-cli board list` will show the port as "Unknown" — that's normal, the Inkplate FQBN can't be auto-detected.

```bash
# Compile (run from repo root)
arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10 .

# Flash
arduino-cli upload --fqbn Croduino_Boards:Inkplate:Inkplate10 --port /dev/ttyUSB0 .

# Read serial output (the arduino-cli monitor produces no output; use cat directly)
stty -F /dev/ttyUSB0 115200 raw && cat /dev/ttyUSB0

# Format (Google style, 120-char limit per .clang-format)
clang-format -i src/**/*.cpp src/**/*.h
```

**Expected boot output** (normal operation):
```
Waiting for WiFi to connect. connected
Waiting for NTP time sync: I: Current time: ...
I: [HTTPS] GET... code: 200
```

The `Wavefrom load failed! Upload new waveform in EEPROM.` warning on boot is a pre-existing Inkplate e-paper waveform message — not an error.

## Architecture

All application logic lives in `setup()` in `Weather_Station_2.ino`. `loop()` is intentionally empty — the device deep-sleeps and reboots each cycle rather than looping.

**Data flow:**
1. Boot → init display → connect WiFi (via `Network`) → sync NTP
2. Fetch weather from `api.weather.gov` (via `CurrentConditions`)
3. Render display: weather left, cat image right, battery/temp bottom
4. Deep sleep 600s (normal) or 300s (on error)

**Key non-obvious details:**

- `Weather_Station_2.ino` includes a `std::make_unique` polyfill (lines 9–15) because the ESP32 Arduino core doesn't provide it.
- `Kitties` uses `RTC_DATA_ATTR` to persist the image rotation counter across deep sleep cycles without hitting flash.
- `CACerts` is a static `std::map<String, const char*>` mapping hostnames to PEM certs embedded as PROGMEM strings. Used by `Network` to configure `WiFiClientSecure` before each HTTPS request.
- WiFi credentials are hardcoded in `Weather_Station_2.ino` — there is no config file.
- Weather station is hardcoded as `KBFI` (Seattle/Boeing Field) in `CurrentConditions.cpp`.

## Certificate Updates

When a weather.gov cert expires, fetch the current intermediate CA from the live server and update `src/security/CACerts.cpp` manually (add new PROGMEM constant, update the domain→cert map). The `cert_collector.py` tool can automate this:

```bash
python3 tools/cert_collector.py -u src/security/CACerts.cpp -f src/security/CACerts.h
```

Also save the new `.pem` to `assets/certificates/` for reference.

## Adding Cat Images

1. Add PNG to `assets/images/`
2. Run `python3 tools/image_converter.py` (edit the script to point at your image)
3. Update `src/display/KittyPics.h/.cpp` with the new bitmap array
4. Register the image in `src/display/Kitties.h/.cpp`
