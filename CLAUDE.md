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

# Read serial output
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
# Note: if the device is in deep sleep, no output will appear until the physical
# wakeup button on the board is pressed.

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

## weather.gov API Reference

The device uses the National Weather Service API at `https://api.weather.gov`.

**OpenAPI spec:** `https://api.weather.gov/openapi.json`

### Endpoint used

```
GET /stations/{stationId}/observations/latest
```

Optional query param: `require_qc` (boolean) — omitted, not currently used.

### Response structure

The response is a GeoJSON Feature. Relevant fields are under `properties`:

| Field | Type | Units | Notes |
|---|---|---|---|
| `textDescription` | string | — | Human-readable summary, e.g. `"Clear"` |
| `rawMessage` | string | — | METAR string; may be empty string (not null) |
| `temperature` | QuantitativeValue | `wmoUnit:degC` | `{ value: 5.0, unitCode: "wmoUnit:degC", qualityControl: "V" }` |
| `dewpoint` | QuantitativeValue | `wmoUnit:degC` | Same structure as temperature |
| `windSpeed` | QuantitativeValue | `wmoUnit:km_h-1` | Fractional km/h, e.g. `7.416` |
| `windDirection` | QuantitativeValue | `wmoUnit:degree_(angle)` | Not currently fetched |
| `windGust` | QuantitativeValue | `wmoUnit:km_h-1` | May be null |
| `barometricPressure` | QuantitativeValue | `wmoUnit:Pa` | Not currently fetched |
| `relativeHumidity` | QuantitativeValue | `wmoUnit:percent` | Not currently fetched |
| `windChill` | QuantitativeValue | `wmoUnit:degC` | Not currently fetched |

QuantitativeValue shape:
```json
{
  "value": 7.416,
  "unitCode": "wmoUnit:km_h-1",
  "qualityControl": "V"
}
```

`qualityControl` values: `"V"` = verified, `"Z"` = no data/null observation.

The code filters the response to only `textDescription`, `rawMessage`, `windSpeed`, `dewpoint`, and `temperature` using ArduinoJson's `DeserializationOption::Filter`.

## Adding Cat Images

1. Add PNG to `assets/images/`
2. Run `python3 tools/image_converter.py` (edit the script to point at your image)
3. Update `src/display/KittyPics.h/.cpp` with the new bitmap array
4. Register the image in `src/display/Kitties.h/.cpp`
