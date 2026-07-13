# Drop Arduino-CMake-Toolchain Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `Arduino-CMake-Toolchain` as the build driver with `arduino-cli compile`/`upload`, invoked from CMake via custom targets, while keeping CMake for codegen (`Layout.cmake`, `Version.cmake`, `user_settings.h.in`) and as the single `cmake --build build` entry point.

**Architecture:** `CMakeLists.txt` drops `CMAKE_TOOLCHAIN_FILE`/`project(... CXX)`/`add_executable`/`target_link_arduino_libraries`/`target_enable_arduino_upload`. Each of the three Arduino targets (`WeatherStation`, `DeviceUnitTests`, `GrayscaleCalibration`) becomes an `add_custom_target(... ALL COMMAND arduino-cli compile ...)`, with a paired `upload-<name>` custom target calling `arduino-cli upload`. The three sketch files revert from `.cpp` to `.ino` (arduino-cli requires the primary sketch file to match its directory name with a `.ino` extension). `tests/unit/` (host GTest) and all codegen are untouched.

**Tech Stack:** CMake 3.16+, arduino-cli 1.3.1, esp32:esp32@3.3.10 core (already installed per issue #27), Ninja.

---

## Errata (found empirically during execution — read before implementing Tasks 4, 5, 9)

Two bugs in this plan's original text were caught during execution and corrected below. Both are documented here so anyone reading this plan later (or re-running it) doesn't reintroduce them.

1. **`$ENV{SERIAL_PORT}` in upload targets is wrong.** Every `upload-<name>` custom target's original text used `-p $ENV{SERIAL_PORT}` directly in the `COMMAND` list. `$ENV{...}` is CMake-language substitution, resolved at `cmake` *configure* time, not at `cmake --build`/execution time — so a user setting `SERIAL_PORT` differently at flash time would be silently ignored. **Fix:** route the port through a shell instead, so it's resolved at execution time: `COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${FQBN_VAR} --input-dir ${BUILD_DIR_VAR} ${SKETCH_DIR_VAR}"`. This is already corrected in Task 2's `upload-WeatherStation` (see commit history) and in the Task 3/4 text below.

2. **arduino-cli requires a sketch's containing directory basename to exactly match its primary `.ino` file's basename** — not just "the primary file must be `.ino`," which is how the rest of this document originally described it. Confirmed by direct, repeatable reproduction: `arduino-cli compile <dir>` always looks for `<dir>/<basename of dir>.ino`, regardless of what other files exist inside. This broke two things the original plan text got wrong:
   - `tests/calibration/GrayscaleCalibration.ino` (directory `calibration` ≠ file `GrayscaleCalibration`) — **fixed by renaming the directory itself**: `tests/calibration/` → `tests/GrayscaleCalibration/`, with `tests/CMakeLists.txt`'s `add_subdirectory(calibration)` updated to `add_subdirectory(GrayscaleCalibration)`.
   - `tests/device/unit/DeviceUnitTests.ino` has the identical problem (directory `unit` ≠ file `DeviceUnitTests`) — Task 4 below is corrected to rename `tests/device/unit/` → `tests/device/DeviceUnitTests/` instead, with `tests/device/CMakeLists.txt`'s `add_subdirectory(unit)` updated accordingly.
   - **This also affects `WeatherStation` itself**, but not via a source-tree rename: `build.sh`'s container invocation always mounts the repo at a fixed path `/project` inside the container (see `run_in_container()`), so arduino-cli always looks for `/project/project.ino` — which never exists, since the real file is `Weather_Station_2.ino`. Task 5 below is corrected to change the container mount point to a path whose basename is `Weather_Station_2` instead of the fixed `/project`.

## Context: what's already done (issue #27)

These are already committed on this branch and unaffected by this plan:
- `cmake/inkplate10-board.txt` — custom `Inkplate10` board entry under `esp32:esp32`
- `Containerfile` — installs `esp32:esp32@3.3.10`, appends the custom board entry
- Confirmed via spike: `arduino-cli compile --fqbn esp32:esp32:Inkplate10 --board-options PSRAM=enabled,PartitionScheme=huge_app,CPUFreq=240` resolves the board/platform correctly with zero property errors

One thing from #27 gets reverted here: the `platform.txt` nested-brace `sed` patch in the `Containerfile` (Task 6 below) — it exists only to help `Arduino-CMake-Toolchain` parse `platform.txt`, which after this plan no longer touches it at all.

## Reference: verified arduino-cli invocation

Confirmed empirically during spec/planning (see `docs/superpowers/specs/2026-07-12-drop-arduino-cmake-toolchain-design.md`):

```bash
arduino-cli compile --fqbn esp32:esp32:Inkplate10 \
  --board-options PSRAM=enabled,PartitionScheme=huge_app,CPUFreq=240 \
  --build-property 'compiler.cpp.extra_flags=-I<path-to-generated-headers>' \
  --build-property 'compiler.c.extra_flags=-I<path-to-generated-headers>' \
  --build-path <build-dir> \
  --only-compilation-database \
  <sketch-dir>
```

- `--board-options` values map 1:1 to the menu names/options defined in `cmake/inkplate10-board.txt`.
- `compiler.cpp.extra_flags`/`compiler.c.extra_flags` default to empty in `platform.txt` — safe to set without clobbering anything, and this is exactly how we make `${CMAKE_BINARY_DIR}/generated/weather_station_2/*.h` visible to the sketch.
- `--only-compilation-database` alone just emits `compile_commands.json`; a normal compile (without that flag) still produces `compile_commands.json` as a side effect, so we only need the flag for a fast DB-only refresh if we want one — not required for this plan.
- `--board-options PartitionScheme=huge_app` (or `default`) is sufficient; **no manual partition-table copy step is needed** — arduino-cli generates `partitions.csv` and `<sketch>.ino.partitions.bin` itself. This lets us delete the `file(COPY...)`/`file(RENAME...)` blocks in all three current `CMakeLists.txt` files.
- Output binaries land at `<build-dir>/<SketchName>.ino.bin` (app), `.ino.bootloader.bin`, `.ino.partitions.bin`, `.ino.merged.bin` (single-file flashable image).
- Upload: `arduino-cli upload -p <port> -b esp32:esp32:Inkplate10 --input-dir <build-dir> <sketch-dir>` (does not recompile; reads the binaries already in `--input-dir`).

## File Structure

```
Weather_Station_2.ino                    ← renamed from .cpp (Task 1)
CMakeLists.txt                           ← rewritten: custom targets instead of add_executable (Task 2)
cmake/BoardOptions.cmake                 ← rewritten: plain board-options string, no toolchain cache vars (Task 2)
tests/calibration/GrayscaleCalibration.ino  ← renamed from .cpp (Task 3)
tests/calibration/CMakeLists.txt         ← rewritten: custom target (Task 3)
tests/device/unit/DeviceUnitTests.ino    ← renamed from .cpp (Task 4)
tests/device/unit/CMakeLists.txt         ← rewritten: custom target (Task 4)
build.sh                                 ← configure step drops toolchain-file flag (Task 5)
Containerfile                            ← drop Arduino-CMake-Toolchain clone + platform.txt patch (Task 6)
CLAUDE.md                                ← no functional change expected; verify and update if wrong (Task 7)
```

`tests/unit/CMakeLists.txt` (host GTest project) and `cmake/Layout.cmake`/`cmake/Version.cmake`/`src/user_settings.h.in` are **not touched** by this plan.

---

## Task 1: Revert `Weather_Station_2.cpp` → `Weather_Station_2.ino`

**Files:**
- Rename: `Weather_Station_2.cpp` → `Weather_Station_2.ino`

- [ ] **Step 1: Rename the file**

```bash
git mv Weather_Station_2.cpp Weather_Station_2.ino
```

- [ ] **Step 2: Remove the now-unnecessary manual `#include <Arduino.h>`**

`.ino` files get this injected automatically by arduino-cli's sketch preprocessing (this is exactly why the original CMake migration had to add it manually — that reason no longer applies). Open `Weather_Station_2.ino` and remove the line:

```cpp
#include <Arduino.h>
```

Leave every other include and all logic untouched — this file has no helper functions requiring forward-declaration, so `.ino` preprocessing has nothing else to change about it.

- [ ] **Step 3: Commit**

```bash
git add Weather_Station_2.ino
git commit -m "$(cat <<'EOF'
refactor: revert Weather_Station_2.cpp to .ino for arduino-cli compile

arduino-cli requires a sketch's primary file to be named
<directory-name>.ino; this was renamed to .cpp for
Arduino-CMake-Toolchain, which is being dropped as the build driver.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(Task 2 will update `CMakeLists.txt` to reference the new filename — the build is expected to be broken between this commit and the end of Task 2. That's fine; this is mid-refactor, not a release point.)

---

## Task 2: Rewrite root `CMakeLists.txt` and `cmake/BoardOptions.cmake`

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `cmake/BoardOptions.cmake`

- [ ] **Step 1: Rewrite `cmake/BoardOptions.cmake` as a plain board-options string**

Replace its entire contents:

```cmake
# ABOUTME: Board identity and options for the Inkplate 10, passed to arduino-cli compile/upload.
# ABOUTME: Single source of truth for FQBN + board-options — see cmake/inkplate10-board.txt for the board definition itself.
set(INKPLATE10_FQBN "esp32:esp32:Inkplate10")
set(INKPLATE10_BOARD_OPTIONS "PSRAM=enabled,PartitionScheme=huge_app,CPUFreq=240")
```

(`DeviceUnitTests` and `GrayscaleCalibration` use the same FQBN with `PartitionScheme=default` instead of `huge_app` — that's set locally in their own `CMakeLists.txt` in Tasks 3–4, not here.)

- [ ] **Step 2: Rewrite `CMakeLists.txt`**

Replace its entire contents:

```cmake
# CMakeLists.txt
# ABOUTME: Root CMake build file for the Weather Station firmware.
# ABOUTME: Drives arduino-cli compile/upload as custom targets; CMake itself only handles codegen.
cmake_minimum_required(VERSION 3.16)
project(WeatherStation NONE)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.cmake")
    message(FATAL_ERROR "config.cmake not found. Copy config.cmake.example to config.cmake and fill in your values.")
endif()
include("${CMAKE_SOURCE_DIR}/config.cmake")
include(cmake/Layout.cmake)
include(cmake/Version.cmake)
include(cmake/BoardOptions.cmake)

set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR}/weather_station_2)
configure_file(src/user_settings.h.in ${GENERATED_INCLUDE_DIR}/weather_station_2/user_settings.h)

# ── Firmware target ───────────────────────────────────────────────────────────
set(WEATHER_STATION_BUILD_DIR "${CMAKE_BINARY_DIR}/WeatherStation")

add_custom_target(WeatherStation ALL
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options ${INKPLATE10_BOARD_OPTIONS}
        --build-property "compiler.cpp.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${GENERATED_INCLUDE_DIR}"
        --build-path ${WEATHER_STATION_BUILD_DIR}
        ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling WeatherStation firmware via arduino-cli"
    VERBATIM
)

add_custom_target(upload-WeatherStation
    COMMAND arduino-cli upload
        -p $ENV{SERIAL_PORT}
        -b ${INKPLATE10_FQBN}
        --input-dir ${WEATHER_STATION_BUILD_DIR}
        ${CMAKE_SOURCE_DIR}
    DEPENDS WeatherStation
    COMMENT "Uploading WeatherStation firmware"
    VERBATIM
)

# ── Test sketches ─────────────────────────────────────────────────────────────
add_subdirectory(tests)
```

Notes on what changed and why, for the engineer implementing this:
- `project(WeatherStation NONE)` — no language means no compiler detection, no toolchain file, at all. This is the single biggest simplification: we're no longer asking CMake to understand a cross-compiler.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is gone — that was for CMake's own native compiler integration, which no longer exists. `compile_commands.json` now comes from arduino-cli itself, landing at `${WEATHER_STATION_BUILD_DIR}/compile_commands.json` (see Task 8 for the symlink update).
- The old `target_include_directories(WeatherStation PRIVATE src ${CMAKE_BINARY_DIR}/generated)` is replaced by the `compiler.cpp.extra_flags`/`compiler.c.extra_flags` build-properties — `src/` itself needs no explicit include flag because arduino-cli's sketch convention automatically adds a top-level `src/` directory to the include path and compiles its `.cpp` files.
- The old manual `huge_app.csv` → `partitions.csv` copy block is gone entirely — `--board-options PartitionScheme=huge_app` makes arduino-cli generate this itself.
- `$ENV{SERIAL_PORT}` — `build.sh flash` already exports `SERIAL_PORT` before invoking this target; unchanged behavior from the reader's perspective.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt cmake/BoardOptions.cmake
git commit -m "$(cat <<'EOF'
refactor: drive WeatherStation build via arduino-cli compile custom target

Replaces add_executable/target_link_arduino_libraries/
target_enable_arduino_upload (Arduino-CMake-Toolchain) with a plain
add_custom_target invoking arduino-cli compile directly. CMake no
longer needs CMAKE_TOOLCHAIN_FILE or CXX compiler detection at all —
its remaining job is codegen (Layout.cmake, Version.cmake,
user_settings.h.in) plus being the single `cmake --build` entry point.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(Still broken at this point — `tests/` subdirectories reference the old toolchain functions. Tasks 3–4 fix that.)

---

## Task 3: Rewrite `tests/calibration/` (`GrayscaleCalibration`)

**Files:**
- Rename: `tests/calibration/GrayscaleCalibration.cpp` → `tests/calibration/GrayscaleCalibration.ino`
- Modify: `tests/calibration/CMakeLists.txt`

- [ ] **Step 1: Rename and strip the manual Arduino.h include if present**

```bash
git mv tests/calibration/GrayscaleCalibration.cpp tests/calibration/GrayscaleCalibration.ino
```

Check the top of the file — it currently has `#include <Arduino.h>`. Remove that line for the same reason as Task 1 (redundant once this is a real `.ino` file).

- [ ] **Step 2: Rewrite `tests/calibration/CMakeLists.txt`**

```cmake
# CMakeLists.txt
# ABOUTME: Build target for the grayscale calibration test sketch.
# ABOUTME: Standalone Inkplate target with no network or sensor dependencies.
set(CALIBRATION_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/GrayscaleCalibration")

add_custom_target(GrayscaleCalibration ALL
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "PSRAM=enabled,PartitionScheme=default,CPUFreq=240"
        --build-path ${CALIBRATION_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling GrayscaleCalibration via arduino-cli"
    VERBATIM
)

add_custom_target(upload-GrayscaleCalibration
    COMMAND arduino-cli upload
        -p $ENV{SERIAL_PORT}
        -b ${INKPLATE10_FQBN}
        --input-dir ${CALIBRATION_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS GrayscaleCalibration
    COMMENT "Uploading GrayscaleCalibration"
    VERBATIM
)
```

This target had no generated-header dependency before and doesn't need one now — no `extra_flags` build-properties required.

- [ ] **Step 3: Commit**

```bash
git add tests/calibration/GrayscaleCalibration.ino tests/calibration/CMakeLists.txt
git commit -m "$(cat <<'EOF'
refactor: drive GrayscaleCalibration build via arduino-cli compile

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Rewrite `tests/device/unit/` (`DeviceUnitTests`)

**Corrected per Errata above: the directory is renamed, not just the file** — arduino-cli requires the containing directory's basename to match the `.ino` file's basename, and `unit` ≠ `DeviceUnitTests`.

**Files:**
- Rename: `tests/device/unit/` → `tests/device/DeviceUnitTests/` (directory, carrying both files with it)
- Rename: `tests/device/DeviceUnitTests/DeviceUnitTests.cpp` → `tests/device/DeviceUnitTests/DeviceUnitTests.ino`
- Modify: `tests/device/DeviceUnitTests/CMakeLists.txt`
- Modify: `tests/device/CMakeLists.txt` (the parent — its `add_subdirectory(unit)` must become `add_subdirectory(DeviceUnitTests)`)

- [ ] **Step 1: Rename the directory, then the file, then strip the manual Arduino.h include**

```bash
git mv tests/device/unit tests/device/DeviceUnitTests
git mv tests/device/DeviceUnitTests/DeviceUnitTests.cpp tests/device/DeviceUnitTests/DeviceUnitTests.ino
```

Remove `#include <Arduino.h>` from the top of the file.

- [ ] **Step 2: Update the parent `tests/device/CMakeLists.txt`**

Change its `add_subdirectory(unit)` to `add_subdirectory(DeviceUnitTests)`.

- [ ] **Step 3: Rewrite `tests/device/DeviceUnitTests/CMakeLists.txt`**

```cmake
# CMakeLists.txt
# ABOUTME: Build target for device unit tests using AUnit and the gtest adapter.
# ABOUTME: Flash and monitor serial output at 115200 baud for PASS/FAIL results.
set(DEVICE_TESTS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/DeviceUnitTests")

add_custom_target(DeviceUnitTests
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "PSRAM=enabled,PartitionScheme=default,CPUFreq=240"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR}"
        --build-path ${DEVICE_TESTS_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling DeviceUnitTests via arduino-cli"
    VERBATIM
)

add_custom_target(upload-DeviceUnitTests
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${DEVICE_TESTS_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS DeviceUnitTests
    COMMENT "Uploading DeviceUnitTests"
    VERBATIM
)
```

(Note the `upload-DeviceUnitTests` target already uses the corrected `sh -c "... \"$SERIAL_PORT\" ..."` pattern from the Errata section, not raw `$ENV{SERIAL_PORT}`.)

Notes:
- `DeviceUnitTests` is deliberately **not** added to the `ALL` target (matches today's behavior — `build.sh build-device-tests` builds it explicitly via `cmake --build build --target DeviceUnitTests`, it's not part of the default `WeatherStation`-only build).
- `DeviceUnitTests.ino` includes project headers like `"display/Kitties.h"` and `"network/CurrentConditions.h"` using paths relative to `src/` (see current file), so `-I${CMAKE_SOURCE_DIR}/src` is needed here in addition to the generated-headers path — unlike `GrayscaleCalibration`, which doesn't reach into `src/`.

- [ ] **Step 4: Commit**

```bash
git add tests/device/CMakeLists.txt tests/device/DeviceUnitTests/DeviceUnitTests.ino tests/device/DeviceUnitTests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
refactor: drive DeviceUnitTests build via arduino-cli compile

Also renames tests/device/unit/ to tests/device/DeviceUnitTests/,
since arduino-cli requires a sketch's containing directory basename to
match its primary .ino file's basename.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Update `build.sh`

**Files:**
- Modify: `build.sh`

- [ ] **Step 1: Drop the toolchain-file flag from `configure`**

Change:

```bash
    configure)
        run_in_container \
            "cmake \
              -DCMAKE_TOOLCHAIN_FILE=/opt/arduino-cmake-toolchain/Arduino-toolchain.cmake \
              -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
              -B build \
              -G Ninja"
        ;;
```

to:

```bash
    configure)
        run_in_container "cmake -B build -G Ninja"
        ;;
```

`-DARDUINO_BOARD_OPTIONS_FILE` is gone too — `cmake/BoardOptions.cmake` is now just `include()`-d directly by `CMakeLists.txt` (Task 2), not consumed as a special toolchain-file variable.

- [ ] **Step 2: Verify the rest of `build.sh` needs no changes**

`build`, `flash`, `monitor`, `test-host`, `build-device-tests`, `flash-device-tests` all reference target names (`upload-WeatherStation`, `DeviceUnitTests`, `upload-DeviceUnitTests`) that still exist under the new scheme (Tasks 2 and 4 preserve those names) — read through the file once after editing to confirm no other reference to the toolchain path remains.

- [ ] **Step 3: Commit**

```bash
git add build.sh
git commit -m "$(cat <<'EOF'
refactor: drop CMAKE_TOOLCHAIN_FILE from configure step

No longer needed now that CMakeLists.txt drives the Arduino build via
arduino-cli compile custom targets instead of Arduino-CMake-Toolchain.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Simplify `Containerfile`

**Files:**
- Modify: `Containerfile`

- [ ] **Step 1: Remove the `Arduino-CMake-Toolchain` clone step**

Delete this block entirely:

```dockerfile
# Pin Arduino-CMake-Toolchain at the commit previously tracked as a submodule.
# This repo has no release tags, so a commit hash is the only stable pin.
# --no-tags suppresses tag fetching for a leaner clone.
RUN git clone --no-tags https://github.com/a9183756-gh/Arduino-CMake-Toolchain.git \
        /opt/arduino-cmake-toolchain \
    && cd /opt/arduino-cmake-toolchain \
    && git checkout e745a9bed3c3fb83442d55bf05630f31574674f2
```

- [ ] **Step 2: Remove the `platform.txt` nested-brace patch**

Delete this block (added for #27 specifically to work around `Arduino-CMake-Toolchain`'s property expander; unnecessary once arduino-cli — which handles this syntax natively — is the sole consumer of `platform.txt`):

```dockerfile
# esp32:esp32's platform.txt builds some property names by nesting one variable inside
# another, e.g. compiler.path={tools.{build.tarch}-esp-elf-gcc.path}/bin/, to route per-chip
# (xtensa vs riscv) toolchains and library sets. Arduino-CMake-Toolchain's property expander
# only does single-level, non-nested {var} substitution, so it can't resolve these and leaves
# the literal "{tools.{build.tarch}-esp-elf-gcc.path}" in the compiler path, breaking the
# build. This project only ever targets classic ESP32 (build.tarch=xtensa, build.mcu=esp32,
# build.chip_variant={build.mcu}=esp32), so these can be safely flattened to their resolved
# values for that fixed target.
RUN PLATFORM_TXT=/root/.arduino15/packages/esp32/hardware/esp32/3.3.10/platform.txt \
    && sed -i \
        -e 's/{runtime\.tools\.{build\.chip_variant}-libs\.path}/{runtime.tools.esp32-libs.path}/g' \
        -e 's/{tools\.{build\.tarch}-esp-elf-gcc\.path}/{tools.xtensa-esp-elf-gcc.path}/g' \
        -e 's/{tools\.{build\.tarch}-esp-elf-gdb\.path}/{tools.xtensa-esp-elf-gdb.path}/g' \
        -e 's/{build\.extra_flags\.{build\.mcu}}/{build.extra_flags.esp32}/g' \
        "$PLATFORM_TXT"
```

- [ ] **Step 3: Also drop the CMake/Ninja apt packages if nothing else needs them**

Check: `cmake` and `ninja-build` are still needed (CMake still drives codegen + orchestration + `tests/unit`'s host GTest project, which is a real CMake project). **Do not remove these packages** — only the toolchain clone and the platform.txt patch go away.

- [ ] **Step 4: Rebuild the image and confirm it still builds cleanly**

```bash
podman build -t weather-station-builder -f Containerfile .   # or docker build
```
Expected: build completes successfully; no reference to `/opt/arduino-cmake-toolchain` remains anywhere in the log.

- [ ] **Step 5: Commit**

```bash
git add Containerfile
git commit -m "$(cat <<'EOF'
refactor: remove Arduino-CMake-Toolchain and its platform.txt patch

Both existed solely to support Arduino-CMake-Toolchain as the build
driver, which CMakeLists.txt no longer uses (arduino-cli compile now
drives the build directly and needs no help parsing platform.txt).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Verify `CLAUDE.md` needs no functional changes

**Files:**
- Modify (only if verification finds a discrepancy): `CLAUDE.md`

- [ ] **Step 1: Re-read the Build/Flash/Serial section**

The documented user-facing commands (`./build.sh configure`, `./build.sh build`, `./build.sh flash`, `./build.sh monitor`, etc.) are unchanged by this plan — only what happens underneath `build.sh` changed. Read `CLAUDE.md`'s "Build, Flash & Serial" section and confirm every command listed there still works as documented after Tasks 1–6.

- [ ] **Step 2: Update if anything's wrong**

If any documented command or expected output no longer matches reality (e.g., a build warning changed, a step was removed), update `CLAUDE.md` to match. Do not add new content beyond what's needed to keep the doc accurate.

- [ ] **Step 3: Commit if changed**

```bash
git add CLAUDE.md
git commit -m "$(cat <<'EOF'
docs: update CLAUDE.md for arduino-cli-driven build

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(Skip this commit entirely if Step 1 found nothing to change.)

---

## Task 8: Update `compile_commands.json` symlink instructions

**Files:**
- Modify: `CLAUDE.md` (if it documents the symlink) or wherever the clangd setup instructions live — check `docs/superpowers/specs/2026-03-24-cmake-migration-design.md`'s "Enable clangd" section for where this was originally documented, and find the equivalent current-day location (likely `CLAUDE.md` or `docs/README.md`).

- [ ] **Step 1: Find where the `compile_commands.json` symlink is documented today**

```bash
grep -rn "compile_commands" CLAUDE.md docs/ 2>/dev/null
```

- [ ] **Step 2: Update the symlink target**

The symlink previously pointed at `build/compile_commands.json` (produced by CMake's native `CMAKE_EXPORT_COMPILE_COMMANDS`). It now needs to point at `build/WeatherStation/compile_commands.json` (produced by arduino-cli inside the `WeatherStation` custom target's `--build-path`, per Task 2). Update wherever this is documented:

```bash
ln -sf build/WeatherStation/compile_commands.json compile_commands.json
```

- [ ] **Step 3: Verify clangd actually works**

After a successful `./build.sh build` (Task 9), create the symlink and open `Weather_Station_2.ino` (or a `src/*.cpp` file) in an editor with clangd configured (Helix, per the project's stated environment) and confirm it resolves includes/diagnostics without errors about missing `compile_commands.json` entries.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
docs: point compile_commands.json symlink at arduino-cli's output path

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: End-to-end verification

No new files — this is a verification pass over everything above.

- [ ] **Step 1: Clean rebuild the container image**

```bash
podman build -t weather-station-builder -f Containerfile .
```
Expected: succeeds, no `Arduino-CMake-Toolchain` or `platform.txt` patch steps appear in the log.

- [ ] **Step 2: Configure and build**

```bash
rm -rf build
./build.sh configure
./build.sh build
```
Expected: `WeatherStation` target compiles successfully via `arduino-cli compile` (visible in the build log), producing `build/WeatherStation/WeatherStation.ino.bin` and `build/WeatherStation/compile_commands.json`.

- [ ] **Step 3: Build the device test targets explicitly**

```bash
./build.sh build-device-tests
cmake --build build --target GrayscaleCalibration
```
Expected: both compile successfully, each producing its own `.ino.bin` under its respective build subdirectory.

- [ ] **Step 4: Run host unit tests (unaffected control check)**

```bash
./build.sh test-host
```
Expected: passes exactly as before this plan — `tests/unit/` never touched Arduino-CMake-Toolchain, so this is a regression check, not new functionality.

- [ ] **Step 5: Flash and confirm boot, if hardware is available**

```bash
./build.sh flash
./build.sh monitor
```
Expected boot output per `CLAUDE.md`:
```
Waiting for WiFi to connect. connected
Waiting for NTP time sync: I: Current time: ...
I: [HTTPS] GET... code: 200
```
Note: at this point the firmware still links against `InkplateLibrary@10.2.2`, which has known, separate compilation issues against the newer compiler (the NBSP-character bug found during the #27/#28 spike). If the real `WeatherStation` build fails on that bug rather than succeeding, that is **expected and out of scope for this plan** — it's issue #28's fork-and-patch work. Confirm the failure (if any) is specifically that pre-existing library bug and not a regression in the arduino-cli-driven build itself (e.g., by checking the error matches the `Adafruit_GFX.h:8` extended-character error from the spec's spike, not a board/property/include-path error).

- [ ] **Step 6: Final review commit (if any cleanup needed)**

If verification surfaces small fixes (typos, a missed reference to the old toolchain path), fix and commit them individually with clear messages — do not bundle unrelated cleanup into this step.

---

## Out of Scope (unchanged from the spec)

- Fixing InkplateLibrary's own compilation issues against the newer core/compiler — issue #28.
- Any change to `cmake/Layout.cmake`, `cmake/Version.cmake`, or `src/user_settings.h.in` beyond what's needed to keep their generated headers visible to arduino-cli (handled via the `extra_flags` build-properties in Task 2).
