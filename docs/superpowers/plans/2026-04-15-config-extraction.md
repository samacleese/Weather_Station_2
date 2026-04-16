# Config Extraction Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move hardcoded WiFi credentials, weather station ID, and battery logger URL out of source files into a git-ignored `config.cmake`, with CMake generating `${CMAKE_BINARY_DIR}/generated/config.h` for use by the firmware.

**Architecture:** A `config.cmake` file (git-ignored) holds local environment values as CMake variables. `CMakeLists.txt` includes it with a `FATAL_ERROR` guard, then calls `configure_file` to substitute those variables into `src/config.h.in`, producing `${CMAKE_BINARY_DIR}/generated/config.h`. Source files `#include "config.h"` directly.

**Tech Stack:** CMake `configure_file`, C++11 `#define` macros, arduino-cmake-toolchain, ESP32/Inkplate 10.

**Spec:** `docs/superpowers/specs/2026-04-15-config-extraction-design.md`

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `config.cmake.example` | Committed template with placeholder values — the only onboarding reference |
| Create (git-ignored) | `config.cmake` | Local environment values; never committed |
| Create | `src/config.h.in` | CMake template; `@VAR@` tokens substituted at configure time |
| Modify | `CMakeLists.txt` | Include guard, `configure_file`, extended `target_include_directories` |
| Modify | `.gitignore` | Exclude `config.cmake` |
| Modify | `Weather_Station_2.cpp` | Consume `WIFI_SSID`, `WIFI_PASSWORD`, `WEATHER_STATION_ID`, `BATTERY_LOGGER_URL` from `config.h` |
| Modify | `CurrentConditions.h` | Remove `= "KBFI"` default; station now always passed explicitly |
| No change | `CurrentConditions.cpp` | Station used only via `m_station` set in constructor — unaffected |

---

## Chunk 1: Config files and .gitignore

### Task 1: Create `config.cmake.example`

**Files:**
- Create: `config.cmake.example`

- [ ] **Step 1: Create the example file**

```cmake
# ABOUTME: Example configuration file for Weather Station.
# ABOUTME: Copy to config.cmake, fill in real values, and never commit config.cmake.
set(WIFI_SSID "your-ssid-here")
set(WIFI_PASSWORD "your-password-here")
set(WEATHER_STATION_ID "XXXX")
set(BATTERY_LOGGER_URL "http://your-server/path")
```

- [ ] **Step 2: Create your local `config.cmake`**

Copy `config.cmake.example` to `config.cmake` and substitute the real values currently hardcoded in:
- `Weather_Station_2.cpp:44` — `WIFI_SSID` and `WIFI_PASSWORD`
- `CurrentConditions.h:24` — `WEATHER_STATION_ID` (default value `"KBFI"`)
- `Weather_Station_2.cpp:221` — `BATTERY_LOGGER_URL`

```bash
cp config.cmake.example config.cmake
# edit config.cmake with real values
```

- [ ] **Step 3: Add `config.cmake` to `.gitignore`**

In `.gitignore`, add a line after the existing entries:

```
config.cmake
```

- [ ] **Step 4: Verify `config.cmake` is ignored**

```bash
git status
```

Expected: `config.cmake` does **not** appear in untracked files. `config.cmake.example` and `.gitignore` do appear as modified/untracked.

- [ ] **Step 5: Commit**

```bash
git add config.cmake.example .gitignore
git commit -m "feat: add config.cmake.example and gitignore entry for issue #10"
```

---

## Chunk 2: CMake wiring

### Task 2: Create `src/config.h.in` and wire `CMakeLists.txt`

**Files:**
- Create: `src/config.h.in`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `src/config.h.in`**

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

- [ ] **Step 2: Add the `config.cmake` guard and include to `CMakeLists.txt`**

Insert these lines after `project(WeatherStation CXX)` (line 5) and before `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` (line 7):

```cmake
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.cmake")
    message(FATAL_ERROR "config.cmake not found. Copy config.cmake.example to config.cmake and fill in your values.")
endif()
include(${CMAKE_SOURCE_DIR}/config.cmake)
```

- [ ] **Step 3: Add `configure_file` and update `target_include_directories` in `CMakeLists.txt`**

After the `add_executable(WeatherStation ...)` block, add `configure_file` immediately before the existing `target_include_directories` line, then **replace** that existing line with the expanded form.

Replace:
```cmake
target_include_directories(WeatherStation PRIVATE src)
```

With:
```cmake
configure_file(src/config.h.in ${CMAKE_BINARY_DIR}/generated/config.h)
target_include_directories(WeatherStation PRIVATE src ${CMAKE_BINARY_DIR}/generated)
```

- [ ] **Step 4: Verify configure fails without `config.cmake`**

Clear the build directory and temporarily rename `config.cmake`, then run configure:

```bash
rm -rf build
mv config.cmake config.cmake.bak
cmake \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
  -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
  -B build \
  -G Ninja
```

Expected: CMake exits with:
```
CMake Error at CMakeLists.txt:X (message):
  config.cmake not found. Copy config.cmake.example to config.cmake and fill in your values.
```

Restore:
```bash
mv config.cmake.bak config.cmake
```

- [ ] **Step 5: Verify configure succeeds and generated header is correct**

```bash
cmake \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
  -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
  -B build \
  -G Ninja
```

Expected: Configure completes without errors.

```bash
cat build/generated/config.h
```

Expected: All four `#define` lines show your real values (not `@VAR@` tokens).

- [ ] **Step 6: Commit**

```bash
git add src/config.h.in CMakeLists.txt
git commit -m "feat: wire CMake configure_file for config.h generation (issue #10)"
```

---

## Chunk 3: Source file updates

### Task 3: Update `Weather_Station_2.cpp` and `CurrentConditions.h`

**Files:**
- Modify: `Weather_Station_2.cpp`
- Modify: `CurrentConditions.h`

- [ ] **Step 1: Add `#include "config.h"` to `Weather_Station_2.cpp`**

Add as the first local include, before `#include "src/security/CACerts.h"` (line 24):

```cpp
#include "config.h"
```

- [ ] **Step 2: Replace hardcoded WiFi credentials**

At line 44, replace:
```cpp
auto network = std::make_shared<Network>("NorwegianFish", "rufalina");
```
With:
```cpp
auto network = std::make_shared<Network>(WIFI_SSID, WIFI_PASSWORD);
```

- [ ] **Step 3: Replace hardcoded `BatteryLogger` URL**

At line 221, replace the string literal with `BATTERY_LOGGER_URL`:
```cpp
BatteryLogger batteryLogger(BATTERY_LOGGER_URL);
```

- [ ] **Step 4: Pass `WEATHER_STATION_ID` to `CurrentConditions`**

At line 64, replace:
```cpp
CurrentConditions curr(network);
```
With:
```cpp
CurrentConditions curr(network, WEATHER_STATION_ID);
```

- [ ] **Step 5: Remove the `= "KBFI"` default from `CurrentConditions.h`**

In `CurrentConditions.h` at line 24, replace:
```cpp
CurrentConditions(std::shared_ptr<Network> network, const String station = "KBFI");
```
With:
```cpp
CurrentConditions(std::shared_ptr<Network> network, const String station);
```

- [ ] **Step 6: Compile the firmware**

```bash
cmake --build build
```

Expected: Compiles cleanly with no errors or warnings about undefined macros.

- [ ] **Step 7: Flash and verify on device**

```bash
SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

Press the physical wakeup button if the device is in deep sleep. Expected serial output:
```
Waiting for WiFi to connect. connected
Waiting for NTP time sync: I: Current time: ...
I: [HTTPS] GET... code: 200
```

- [ ] **Step 8: Commit**

```bash
git add Weather_Station_2.cpp src/network/CurrentConditions.h
git commit -m "$(cat <<'EOF'
feat: replace hardcoded config values with config.h macros

Closes #10
EOF
)"
```
