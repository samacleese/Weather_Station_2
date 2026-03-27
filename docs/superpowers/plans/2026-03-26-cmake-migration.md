# CMake Build Migration Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `arduino-cli compile` with CMake + `arduino-cmake-toolchain` as the build driver, producing `compile_commands.json` for clangd/LSP support.

**Architecture:** `arduino-cmake-toolchain` is added as a git submodule and reads the Inkplate board definition from `~/.arduino15`. A committed `cmake/BoardOptions.cmake` pre-selects the Inkplate 10 board so configure works without manual editing. `Weather_Station_2.ino` is renamed to `.cpp` with `#include <Arduino.h>` added.

**Tech Stack:** CMake 3.16+, `arduino-cmake-toolchain` (git submodule), Ninja, `xtensa-esp32-elf-gcc` (from `~/.arduino15/packages/Croduino_Boards`)

**Spec:** `docs/superpowers/specs/2026-03-24-cmake-migration-design.md`

---

> **Branch:** All work must be done on a feature branch. Do not commit to `master` directly.

---

> **Note on testing:** This is an embedded project with no host-side unit tests. Verification at each step is build-level: does CMake configure? Does the build produce a `.bin`? Does `compile_commands.json` contain project source files? Each task ends with a concrete verification command and expected output.

---

## Chunk 1: Infrastructure (submodule, BoardOptions, CMakeLists)

### Task 0: Create feature branch

- [ ] **Step 1: Create and switch to a feature branch**

```bash
git checkout -b 9-cmake-build
```

- [ ] **Step 2: Verify you are on the right branch**

```bash
git branch --show-current
```

Expected: `9-cmake-build`

---

### Task 1: Add arduino-cmake-toolchain submodule

**Files:**
- Create: `cmake/Arduino-CMake-Toolchain/` (git submodule)
- Create: `.gitmodules`

- [ ] **Step 1: Add the submodule**

```bash
git submodule add https://github.com/a9183756-gh/Arduino-CMake-Toolchain.git \
    cmake/Arduino-CMake-Toolchain
```

- [ ] **Step 2: Verify the submodule files are present**

```bash
ls cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake
```

Expected: file exists (no "No such file" error)

- [ ] **Step 3: Commit**

```bash
git add .gitmodules cmake/Arduino-CMake-Toolchain
git commit -m "build: add arduino-cmake-toolchain as git submodule"
```

---

### Task 2: Create cmake/BoardOptions.cmake

**Files:**
- Create: `cmake/BoardOptions.cmake`

- [ ] **Step 1: Create the file**

```cmake
# cmake/BoardOptions.cmake
# ABOUTME: Pre-selected board configuration for Inkplate 10 (ESP32).
# ABOUTME: Passed to CMake via -DARDUINO_BOARD_OPTIONS_FILE to avoid manual board selection.
set(ARDUINO_BOARD "Inkplate 10 (ESP32) [Inkplate.Inkplate10]")
set(ARDUINO_INKPLATE_INKPLATE10_MENU_PSRAM_ENABLED TRUE)
set(ARDUINO_INKPLATE_INKPLATE10_MENU_PARTITIONSCHEME_HUGE_APP TRUE)
set(ARDUINO_INKPLATE_INKPLATE10_MENU_CPUFREQ_240 TRUE)
```

- [ ] **Step 2: Commit**

```bash
git add cmake/BoardOptions.cmake
git commit -m "build: add pre-configured BoardOptions.cmake for Inkplate 10"
```

---

### Task 3: Create CMakeLists.txt

**Files:**
- Create: `CMakeLists.txt`

- [ ] **Step 1: Create the file**

```cmake
# CMakeLists.txt
# ABOUTME: Root CMake build file for the Weather Station firmware.
# ABOUTME: Uses arduino-cmake-toolchain to target the Inkplate 10 (ESP32).
cmake_minimum_required(VERSION 3.16)
project(WeatherStation CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# file(COPY ... RENAME) is not available in CMake 3.16; use two-step copy+rename.
file(COPY
    "${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/huge_app.csv"
    DESTINATION "${CMAKE_BINARY_DIR}"
)
file(RENAME
    "${CMAKE_BINARY_DIR}/huge_app.csv"
    "${CMAKE_BINARY_DIR}/partitions.csv"
)
```

- [ ] **Step 2: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add CMakeLists.txt for arduino-cmake-toolchain build"
```

---

## Chunk 2: Source rename and build verification

### Task 4: Rename .ino to .cpp

**Files:**
- Delete: `Weather_Station_2.ino`
- Create: `Weather_Station_2.cpp`

`arduino-cmake-toolchain` does not support `.ino` files. The rename requires adding `#include <Arduino.h>` as the first line — the Arduino IDE injects this implicitly for `.ino` files, but a plain `.cpp` needs it explicit. No other changes to the file are needed: it has only `setup()` and `loop()`, so no forward declarations are required.

- [ ] **Step 1: Rename via git**

```bash
git mv Weather_Station_2.ino Weather_Station_2.cpp
```

- [ ] **Step 2: Add ABOUTME header and `#include <Arduino.h>` at the top**

Open `Weather_Station_2.cpp`. The current first line is:
```cpp
#include <ArduinoLog.h>
```

Change it to:
```cpp
// ABOUTME: Main firmware entry point for the Weather Station.
// ABOUTME: Initializes hardware, fetches weather, renders display, then deep-sleeps.
#include <Arduino.h>
#include <ArduinoLog.h>
```

- [ ] **Step 3: Verify the file looks correct**

```bash
head -6 Weather_Station_2.cpp
```

Expected:
```
// ABOUTME: Main firmware entry point for the Weather Station.
// ABOUTME: Initializes hardware, fetches weather, renders display, then deep-sleeps.
#include <Arduino.h>
#include <ArduinoLog.h>
#include <Inkplate.h>
#include <rom/rtc.h>
```

- [ ] **Step 4: Commit**

```bash
git add Weather_Station_2.cpp
git commit -m "build: rename .ino to .cpp, add ABOUTME header and explicit Arduino.h include"
```

---

### Task 5: CMake configure

**Files:** (no file changes — verification only)

- [ ] **Step 1: Run CMake configure**

```bash
cmake \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
  -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
  -B build \
  -G Ninja
```

- [ ] **Step 2: Verify configure succeeded**

Expected output ends with something like:
```
-- Build files have been written to: .../Weather_Station_2/build
```

If configure fails with "Board not found": double-check that `arduino-cli core install Croduino_Boards:Inkplate` has been run and `~/.arduino15/packages/Croduino_Boards` exists.

If configure fails with "target_link_arduino_libraries: library X not found": run `arduino-cli lib install <library-name>` and re-run configure. Check `arduino-cli lib list` to see what names the installed libraries use.

- [ ] **Step 3: Verify the Ninja build file exists**

```bash
ls build/build.ninja
```

Expected: file exists.

---

### Task 6: CMake build

**Files:** (no file changes — verification only)

- [ ] **Step 1: Build the firmware**

```bash
cmake --build build
```

- [ ] **Step 2: Verify the binary was produced**

```bash
ls -lh build/WeatherStation.bin
```

Expected: file exists, size ~250–400KB.

If the build fails with a library name resolution error, revisit Task 5 step 2 guidance.

If the build fails with a compiler error in `Weather_Station_2.cpp`, the most likely cause is a missing include or a forward declaration issue. Check the error message — it will name the exact line.

---

### Task 7: Verify compile_commands.json

**Files:** (no file changes — verification only)

- [ ] **Step 1: Confirm the file exists**

```bash
ls build/compile_commands.json
```

- [ ] **Step 2: Confirm it contains project source files (not just Arduino core)**

```bash
grep "Weather_Station_2.cpp" build/compile_commands.json
```

Expected: at least one JSON entry with `"file": ".../Weather_Station_2.cpp"`.

---

## Chunk 3: Repository housekeeping

### Task 8: Update .gitignore

**Files:**
- Modify: `.gitignore`

- [ ] **Step 1: Add build dir and compile_commands symlink to .gitignore**

Current `.gitignore`:
```
*.sw[op]
```

Updated `.gitignore`:
```
*.sw[op]
build/
compile_commands.json
```

- [ ] **Step 2: Create the compile_commands.json symlink**

```bash
ln -sf build/compile_commands.json compile_commands.json
```

- [ ] **Step 3: Verify symlink is gitignored**

```bash
git status compile_commands.json
```

Expected: not listed (gitignored).

- [ ] **Step 4: Commit**

```bash
git add .gitignore
git commit -m "build: gitignore build dir and compile_commands.json symlink"
```

---

### Task 9: Update CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Replace the Build, Flash & Serial section**

Find the current section:
```markdown
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
```

Replace with:
```markdown
## Build, Flash & Serial

The device appears on `/dev/ttyUSB0`. The user must be in the `dialout` group (already configured). `arduino-cli board list` will show the port as "Unknown" — that's normal, the Inkplate FQBN can't be auto-detected.

```bash
# Configure (run once, or after CMakeLists.txt changes)
cmake \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
  -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
  -B build \
  -G Ninja

# Compile
cmake --build build

# Flash (SERIAL_PORT is an environment variable)
SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation

# Read serial output
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
# Note: if the device is in deep sleep, no output will appear until the physical
# wakeup button on the board is pressed.

# Enable clangd/LSP (one-time, after first successful build)
ln -sf build/compile_commands.json compile_commands.json

# Format (Google style, 120-char limit per .clang-format)
clang-format -i src/**/*.cpp src/**/*.h
```
```

- [ ] **Step 2: Update the Architecture section reference to the .ino file**

Find:
```
All application logic lives in `setup()` in `Weather_Station_2.ino`. `loop()` is intentionally empty — the device deep-sleeps and reboots each cycle rather than looping.
```

Replace with:
```
All application logic lives in `setup()` in `Weather_Station_2.cpp`. `loop()` is intentionally empty — the device deep-sleeps and reboots each cycle rather than looping.
```

- [ ] **Step 3: Update the Key non-obvious details reference to the .ino file**

Find:
```
- `Weather_Station_2.ino` includes a `std::make_unique` polyfill (lines 9–15) because the ESP32 Arduino core doesn't provide it.
```

Replace with:
```
- `Weather_Station_2.cpp` includes a `std::make_unique` polyfill (lines 12–18) because the ESP32 Arduino core (`-std=gnu++11`) doesn't provide it.
```

(Line numbers shift by +3: 2 ABOUTME lines + 1 `#include <Arduino.h>` added before the existing content.)

- [ ] **Step 4: Update the WiFi credentials note**

Find:
```
- WiFi credentials are hardcoded in `Weather_Station_2.ino` — there is no config file.
```

Replace with:
```
- WiFi credentials are hardcoded in `Weather_Station_2.cpp` — there is no config file.
```

- [ ] **Step 5: Update docs/CLAUDE.md**

`docs/CLAUDE.md` is a separate documentation file that also contains stale references. Make these replacements:

Find:
```
- **Compile**: `arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10`
- **Upload**: Use Arduino IDE "Upload" button or arduino-cli upload
```

Replace with:
```
- **Compile**: `cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake -B build && cmake --build build`
- **Flash**: `SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation`
```

Also find:
```
- **Weather_Station_2.ino**: Main application file with setup() and empty loop() (uses deep sleep)
```

Replace with:
```
- **Weather_Station_2.cpp**: Main application file with setup() and empty loop() (uses deep sleep)
```

Also find (in Project Structure and any other occurrence):
```
├── Weather_Station_2.ino          # Main Arduino sketch
```

Replace with:
```
├── Weather_Station_2.cpp          # Main firmware entry point
```

Also find:
```
WiFi credentials are hardcoded in Weather_Station_2.ino setup() function:
```

Replace with:
```
WiFi credentials are hardcoded in Weather_Station_2.cpp setup() function:
```

- [ ] **Step 6: Commit**

```bash
git add CLAUDE.md docs/CLAUDE.md
git commit -m "docs: update CLAUDE.md files for CMake migration"
```

---

### Task 10: Update .claude/settings.local.json

**Files:**
- Modify: `.claude/settings.local.json`

- [ ] **Step 1: Replace the arduino-cli compile permission with cmake/ninja**

Current content:
```json
{
  "permissions": {
    "allow": [
      "Bash(arduino-cli compile:*)"
    ],
    "deny": [],
    "ask": []
  }
}
```

Updated content:
```json
{
  "permissions": {
    "allow": [
      "Bash(cmake:*)",
      "Bash(ninja:*)"
    ],
    "deny": [],
    "ask": []
  }
}
```

- [ ] **Step 2: Commit**

```bash
git add .claude/settings.local.json
git commit -m "build: update allowed CLI permissions for CMake build"
```

---

## Final Verification

- [ ] **Clean build from scratch**

```bash
rm -rf build
cmake \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
  -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
  -B build \
  -G Ninja
cmake --build build
```

Expected: configure succeeds, build produces `build/WeatherStation.bin`.

- [ ] **Verify compile_commands.json has project sources**

```bash
grep -c "Weather_Station_2\|CurrentConditions\|Network\|CACerts\|Kitties\|KittyPics\|DisplayLocations" \
  build/compile_commands.json
```

Expected: count ≥ 7 (one entry per source file).

- [ ] **Verify arduino-cli is no longer used for building**

```bash
grep -r "arduino-cli compile\|arduino-cli upload" CLAUDE.md docs/CLAUDE.md
```

Expected: no output.

- [ ] **Manual: verify flash target (requires device connected)**

This step requires the Inkplate 10 to be connected via USB. Run:

```bash
SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation
```

Expected: esptool output showing the binary being written to flash, ending with "Hard resetting via RTS pin...". If the device is not available, skip and note it as pending.
