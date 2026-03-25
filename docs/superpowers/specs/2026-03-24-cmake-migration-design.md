# CMake Build Migration Design

**Issue:** #9 — Switch to CMake-based CLI build
**Date:** 2026-03-24
**Status:** Approved

---

## Problem

The project uses `arduino-cli compile` as its build driver. This works for basic compilation and flashing but provides no `compile_commands.json`, so clangd/LSP does not function in Helix. It also provides no canonical mechanism for the downstream goals of build-time code generation (kitty images, font filtering) and config/version header injection. Those downstream issues (#10, #11, #14) all benefit from a proper CMake build.

## Approach

Replace `arduino-cli compile` with CMake + `arduino-cmake-toolchain` as the build driver. The toolchain reads the Inkplate board definition from `~/.arduino15/packages/Croduino_Boards` — the same location `arduino-cli` uses — and drives `xtensa-esp32-elf-gcc` directly. `arduino-cli` is retained for one-time board and library installation only.

This approach was validated by a spike that confirmed:
- The toolchain correctly enumerates the Inkplate 10 board and all its menu options
- `xtensa-esp32-elf-gcc` is found from the Arduino packages path
- A minimal sketch compiles, links, and produces a flashable `.bin`
- `compile_commands.json` is generated with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- The only wrinkle is a missing `partitions.csv` in the build dir, fixed with one `file(COPY)` line

## Repository Layout Changes

```
cmake/
  Arduino-CMake-Toolchain/   ← git submodule (pinned commit)
  BoardOptions.cmake         ← committed, pre-configured for Inkplate10
CMakeLists.txt               ← new root build file
Weather_Station_2.cpp        ← renamed from Weather_Station_2.ino
build/                       ← gitignored out-of-source build dir
compile_commands.json        ← gitignored symlink → build/compile_commands.json
```

`arduino-cmake-toolchain` does not support `.ino` files. `Weather_Station_2.ino` is renamed to `Weather_Station_2.cpp` and gains `#include <Arduino.h>` at the top. No other changes to that file.

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(WeatherStation CXX)

# ── Firmware target ───────────────────────────────────────────────────────────
add_executable(WeatherStation
    Weather_Station_2.cpp
    src/display/DisplayLocations.cpp
    src/display/Kitties.cpp
    src/display/KittyPics.cpp
    src/network/CurrentConditions.cpp
    src/network/Network.cpp
    src/security/CACerts.cpp
)

target_include_directories(WeatherStation PRIVATE src)

# ── Arduino libraries ─────────────────────────────────────────────────────────
target_link_arduino_libraries(WeatherStation PRIVATE
    core
    Inkplate
    ArduinoJson
    ArduinoLog
    LCBUrl
)

target_enable_arduino_upload(WeatherStation)

# ── Partition table (huge_app) ────────────────────────────────────────────────
file(COPY
    "${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/huge_app.csv"
    DESTINATION "${CMAKE_BINARY_DIR}"
    RENAME partitions.csv
)

# ── compile_commands.json for clangd ─────────────────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

The `ARDUINO_BOARD_RUNTIME_PLATFORM_PATH` variable is set by the toolchain. If the exact name differs, the fallback is a path relative to `CMAKE_TOOLCHAIN_FILE`. Verify during implementation.

## cmake/BoardOptions.cmake

Committed so any checkout configures identically without a generate-edit-regenerate cycle:

```cmake
set(ARDUINO_BOARD "Inkplate 10 (ESP32) [Inkplate.Inkplate10]")
set(ARDUINO_INKPLATE_INKPLATE10_MENU_PSRAM_ENABLED TRUE)
set(ARDUINO_INKPLATE_INKPLATE10_MENU_PARTITIONSCHEME_HUGE_APP TRUE)
set(ARDUINO_INKPLATE_INKPLATE10_MENU_CPUFREQ_240 TRUE)
```

## Toolchain Sourcing

`arduino-cmake-toolchain` is added as a git submodule at `cmake/Arduino-CMake-Toolchain/`. A submodule pins an exact commit, is transparent in `git diff`, and works offline after clone. `FetchContent` is avoided because it requires network access at configure time.

## Build Workflow

### First-time setup
```bash
git clone --recurse-submodules <repo>
arduino-cli core install Croduino_Boards:Inkplate   # if not already installed
arduino-cli lib install InkplateLibrary ArduinoJson ArduinoLog LCBUrl
```

### Configure
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
      -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -B build
```

### Build
```bash
cmake --build build
```

### Flash
```bash
cmake --build build --target upload-WeatherStation SERIAL_PORT=/dev/ttyUSB0
```

### Serial monitor
```bash
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

### Enable clangd (one-time after first build)
```bash
ln -sf build/compile_commands.json compile_commands.json
```

## Files Changed

| File | Change |
|---|---|
| `cmake/Arduino-CMake-Toolchain/` | Add (git submodule) |
| `cmake/BoardOptions.cmake` | Add |
| `CMakeLists.txt` | Add |
| `Weather_Station_2.ino` | Delete |
| `Weather_Station_2.cpp` | Add (renamed + `#include <Arduino.h>`) |
| `.gitignore` | Add `build/` and `compile_commands.json` |
| `CLAUDE.md` | Update build commands |
| `.claude/settings.local.json` | Remove `arduino-cli compile` permission |

## Out of Scope

The following downstream issues are enabled by this migration but not implemented here:

- **#10** — `cmake/config.cmake.in` template for WiFi credentials and station ID
- **#11** — `configure_file` for `src/version.h` from git describe output
- **#14** — `add_custom_command` to generate kitty `.cpp/.h` from PNGs at build time
- **#18** — IWYU pre-commit hook (requires `compile_commands.json`, now available)
- **#15** — Containerized build environment (packages this workflow into a container)

## Acceptance Criteria

- `cmake -B build ... && cmake --build build` produces a flashable `.bin`
- `build/compile_commands.json` is generated and contains entries for project source files
- `cmake --build build --target upload-WeatherStation SERIAL_PORT=/dev/ttyUSB0` flashes the device
- `arduino-cli` is used only for board/library installation, not as the build driver
- `CLAUDE.md` reflects the new build commands
- All existing source files in `src/` are unchanged
