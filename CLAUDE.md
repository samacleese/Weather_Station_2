# CLAUDE.md

Weather Station 2: ESP32 firmware for the Inkplate 10 e-paper display. Shows weather.gov conditions plus rotating cat images, deep-sleeping 10 minutes between updates.

## Build, Flash & Serial

Everything goes through `build.sh` (auto-detects podman/docker; pinned env in `Containerfile`, see `docs/README.md`):

```bash
./build.sh configure | build | flash | monitor | test-host | test-python
./build.sh build-device-tests | flash-device-tests
./build.sh build-spike <Name> | flash-spike <Name>   # spikes live in tests/spikes/<Name>
python3 tools/serial_capture.py --seconds 40         # reset + capture serial (monitor needs a TTY)
SERIAL_PORT=/dev/ttyACM0 ./build.sh flash            # override port
clang-format -i src/**/*.cpp src/**/*.h              # Google style, 120 cols
```

- Device is `/dev/ttyUSB0`; user is already in `dialout`. `arduino-cli board list` shows it as "Unknown", which is normal.
- `config.cmake` is gitignored (WiFi creds, station ID, `BATTERY_LOGGER_URL`, `OTA_MANIFEST_URL`; reaches firmware via generated `src/user_settings.h`). Copy it into new worktrees: `cp config.cmake .worktrees/<branch>/config.cmake`.
- Board: Soldered's `soldered-inkplate-boards` package, FQBN `soldered-inkplate-boards:esp32:Inkplate10` (`cmake/BoardOptions.cmake`). Not `Inkplate10V2`, which is a different pinout. InkplateLibrary pinned to 11.1.2 in `Containerfile`.

Expected boot output:
```
Waiting for WiFi to connect. connected
Waiting for NTP time sync: I: Current time: ...
I: [HTTPS] GET... code: 200
```
`Wavefrom load failed! Upload new waveform in EEPROM.` is a harmless pre-existing Inkplate message.

## Architecture

All logic is in `setup()` in `Weather_Station_2.ino`; `loop()` is intentionally empty. Flow: init display → WiFi → NTP → fetch weather → render (weather left, cat right, battery/temp bottom) → deep sleep 600s (300s on error).

- The `.ino` has a `std::make_unique` polyfill because the ESP32 core lacks it.
- `Kitties` persists its rotation counter across deep sleep with `RTC_DATA_ATTR`.
- `CACerts` is a static `std::map<String, const char*>` of hostname → PROGMEM PEM, used by `Network` to configure `WiFiClientSecure`.
- Station is hardcoded as `KBFI` in `CurrentConditions.cpp`.

## weather.gov API

`GET https://api.weather.gov/stations/{id}/observations/latest` (spec: `https://api.weather.gov/openapi.json`). `CurrentConditions` filters to `textDescription`, `rawMessage`, `windSpeed`, `dewpoint`, `temperature` with ArduinoJson `DeserializationOption::Filter`. Gotchas: `rawMessage` can be `""`; values are SI (degC, km/h); `qualityControl: "Z"` means no data.

## Certificate Updates

When a weather.gov cert expires: `python3 tools/cert_collector.py -u src/security/CACerts.cpp -f src/security/CACerts.h` (or add the PROGMEM constant and map entry by hand). Save the new `.pem` to `assets/certificates/`.

## Adding Cat Images

PNG into `assets/images/` → convert with `tools/image_converter.py` (edit the script's path) → add bitmap to `src/display/KittyPics.h/.cpp` → register in `src/display/Kitties.h/.cpp`.

## Layout

Text positions/fonts live in `layout.json`, generated into `layout.h` at configure time (`cmake/Layout.cmake`). Edit visually with `tools/layout-editor/index.html`. A new field needs all four updated together:

1. `layout.json` (`x`, `y`, `font_size` under the screen)
2. `cmake/Layout.cmake` (`string(JSON ... GET ...)`)
3. `src/layout.h.in` (`@PLACEHOLDER@` → `#define`)
4. `tools/layout-editor/index.html` (UI controls)

Re-run `./build.sh configure` after editing `layout.json`.
