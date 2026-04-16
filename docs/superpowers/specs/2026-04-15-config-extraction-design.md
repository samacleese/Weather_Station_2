# Config Extraction Design

Move hardcoded WiFi credentials, weather station ID, and battery logger URL out of source files into a git-ignored `config.cmake`, generated into a `config.h` header via CMake's `configure_file`.

Closes #10.

---

## Problem

Four environment-specific values are hardcoded in source files and cannot be safely committed:

| Value | Current location |
|---|---|
| `WIFI_SSID` | `Weather_Station_2.cpp:44` |
| `WIFI_PASSWORD` | `Weather_Station_2.cpp:44` |
| `WEATHER_STATION_ID` | `CurrentConditions.h:24` (constructor default) |
| `BATTERY_LOGGER_URL` | `Weather_Station_2.cpp:221` |

## Approach

**CMake `configure_file` pattern** — `config.cmake` sets CMake variables; `CMakeLists.txt` passes them through `configure_file` into `${CMAKE_BINARY_DIR}/generated/config.h`; source files `#include "config.h"`.

Rejected alternatives:
- `-D` flags at configure time: inconvenient, user must re-supply every reconfigure
- `target_compile_definitions`: string escaping is fragile in CMake

## C++ Standard Note

The Inkplate platform compiles with `-std=gnu++11`. `std::string_view` (C++17) is unavailable. Config values are plain `#define` macros.

---

## New Files

### `config.cmake.example` (committed)

```cmake
# Copy this file to config.cmake and fill in real values.
# config.cmake is git-ignored and must never be committed.
set(WIFI_SSID "your-ssid-here")
set(WIFI_PASSWORD "your-password-here")
set(WEATHER_STATION_ID "XXXX")
set(BATTERY_LOGGER_URL "http://your-server/path")
```

### `src/config.h.in` (committed)

```c
// ABOUTME: CMake-generated header providing compile-time configuration values.
// ABOUTME: Values are substituted by CMake at configure time from config.cmake.
#ifndef CONFIG_H
#define CONFIG_H
#define WIFI_SSID "@WIFI_SSID@"
#define WIFI_PASSWORD "@WIFI_PASSWORD@"
#define WEATHER_STATION_ID "@WEATHER_STATION_ID@"
#define BATTERY_LOGGER_URL "@BATTERY_LOGGER_URL@"
#endif  // CONFIG_H
```

### `config.cmake` (git-ignored)

User's local copy of `config.cmake.example` with real values. Never committed.

---

## Changed Files

### `CMakeLists.txt`

1. Near the top (before `add_executable`), include `config.cmake` with a clear error if missing:
   ```cmake
   if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.cmake")
       message(FATAL_ERROR "config.cmake not found. Copy config.cmake.example to config.cmake and fill in your values.")
   endif()
   include(${CMAKE_SOURCE_DIR}/config.cmake)
   ```

2. After `add_executable(WeatherStation ...)`, add `configure_file` and **replace** the existing `target_include_directories` line (currently `target_include_directories(WeatherStation PRIVATE src)`) with the expanded form:
   ```cmake
   configure_file(src/config.h.in ${CMAKE_BINARY_DIR}/generated/config.h)
   target_include_directories(WeatherStation PRIVATE src ${CMAKE_BINARY_DIR}/generated)
   ```
   These two lines belong together: `configure_file` generates the header and `target_include_directories` makes it findable. The existing `target_include_directories` line is removed, not duplicated.

### `.gitignore`

Add `config.cmake`.

### `Weather_Station_2.cpp`

- Add `#include "config.h"`
- Replace `"NorwegianFish"` / `"rufalina"` with `WIFI_SSID` / `WIFI_PASSWORD`
- Replace the `BatteryLogger` URL literal with `BATTERY_LOGGER_URL`
- Pass `WEATHER_STATION_ID` explicitly when constructing `CurrentConditions` (line 64)

### `CurrentConditions.h`

- Remove the `= "KBFI"` default from the constructor declaration (line 24)

### `CurrentConditions.cpp`

No changes required. The station value is used only via `m_station`, which is set by the constructor parameter.

---

## Testing

No automated test framework exists for this target. Verification:

1. CMake configure fails with a clear message if `config.cmake` is absent
2. CMake configure succeeds with `config.cmake` present
3. `build/generated/config.h` contains the correct substituted values
4. Firmware compiles without errors
5. Device boots, connects to WiFi, and fetches weather (serial monitor confirms)
