# Drop Arduino-CMake-Toolchain, Drive the Build via arduino-cli

**Issue:** #27 (follow-on; discovered while implementing the board core upgrade)
**Date:** 2026-07-12
**Status:** Proposed

---

## Problem

Issue #27 replaces the abandoned `Croduino_Boards:Inkplate` board package with Espressif's actively maintained `esp32:esp32` core, to unblock current InkplateLibrary releases. That swap is otherwise complete (custom `cmake/inkplate10-board.txt` board entry, updated `Containerfile`, updated `cmake/BoardOptions.cmake`) but exposed a second, deeper problem: `cmake --build` fails against `esp32:esp32@3.3.10` because of the build driver underneath it, not the board definition.

This project's build is driven by `Arduino-CMake-Toolchain` (`a9183756-gh/Arduino-CMake-Toolchain`, pinned at commit `e745a9b`), a third-party CMake reimplementation of Arduino's `platform.txt`/`boards.txt` property resolution. **That project has had zero commits since June 2020.** Espressif's modern `esp32:esp32` platform.txt uses property patterns that didn't exist in 2020 — most importantly, nested `{...}` interpolation to route between chip variants, e.g.:

```
compiler.path={tools.{build.tarch}-esp-elf-gcc.path}/bin/
```

The toolchain's property expander (`Arduino/Utilities/PropertiesReader.cmake`) only does single-level, non-nested substitution: it scans for the first `{`, then the first `}` after it, which for a nested expression like the above grabs the *inner* closing brace and produces a garbled, unresolvable variable name. This isn't a config mistake on our side — it's a structural limitation of an abandoned dependency meeting a compiler that changed layout after that dependency stopped receiving updates. A second, related failure surfaces past that (a `{build.source.path}` → `<TODO_SOURCE_DIR>` placeholder mechanism that collides with CMake's own internal compiler-check step), reinforcing that this isn't a single fixable property but a class of problem.

Patching `platform.txt` line-by-line (as already done for the nested-brace issue, in the spirit of the project's existing `setInsecure()` sed patch) is a real option, but every patch is against generated code no one but us will ever review, for a dependency several ecosystem-generations behind what it's parsing. That's the same shape of problem `Croduino_Boards` was — just one layer deeper in the stack, and this one is load-bearing for the whole build, not just board metadata.

## Why We Have CMake At All

The original migration to CMake (`docs/superpowers/specs/2026-03-24-cmake-migration-design.md`, issue #9) was not about the build driver per se — it was about three things `arduino-cli compile` alone didn't provide:

1. `compile_commands.json`, so clangd/LSP works in Helix
2. A home for build-time codegen: `layout.json` → `layout.h` (`cmake/Layout.cmake`), git version → `version.h` (`cmake/Version.cmake`), WiFi config → `user_settings.h`
3. A single, scriptable entry point (`cmake --build build`) instead of hand-assembled `arduino-cli` invocations

None of those three things require CMake to *understand Arduino's build recipes*. They only require CMake to run some `configure_file`/`string(JSON...)` steps and then invoke *something* to produce a binary. `arduino-cli` is exactly that something — it's the real, actively maintained implementation of Arduino's property resolution (the same one Espressif's `platform.txt` is written and tested against), and as of the version already pinned in this project's `Containerfile` (1.3.1), it can also emit `compile_commands.json` directly via `--only-compilation-database`.

## Approach

Keep CMake. Change what it's responsible for.

- **CMake keeps:** `configure_file`/`string(JSON...)` codegen (`Layout.cmake`, `Version.cmake`, `user_settings.h.in`), and acts as the single `cmake --build build` entry point.
- **CMake drops:** `CMAKE_TOOLCHAIN_FILE`, `project(... CXX)` compiler detection, `add_executable`, `target_link_arduino_libraries`, `target_enable_arduino_upload` — everything that asks CMake to understand and drive a cross-compiler via Arduino-CMake-Toolchain.
- **arduino-cli becomes the build driver**, invoked from CMake via `add_custom_target`/`add_custom_command`, instead of being used only for one-time board/library installation as it is today.

This was spiked directly (see below) rather than assumed: compiling a minimal Inkplate sketch with `arduino-cli compile --fqbn esp32:esp32:Inkplate10 ...` against the same `esp32:esp32@3.3.10` install used for #27 gets **past all board/platform.txt resolution with zero errors** — it fails only on pre-existing InkplateLibrary source bugs (a stray non-breaking-space character in vendored Adafruit-GFX code, and a downstream missed library auto-detection that appears to be a side effect of that same error aborting arduino-cli's dependency scan early). Those are real, but they're issue #28's territory (forking + patching InkplateLibrary), not a reason to doubt this approach — if anything, they confirm #27/#28 are exactly as orthogonal as originally assessed, since they surface identically whether or not this CMake change happens.

**Practical implication:** the `platform.txt` nested-brace sed patch already added to the `Containerfile` for #27 becomes unnecessary once this change lands, since arduino-cli needs no help resolving its own platform's property syntax. It should be reverted as part of this work, not carried forward as dead weight.

## Design Points

These are decisions to work out during planning, not fully specified here:

1. **The `.ino` rename reverses.** The original CMake migration (#9) renamed `Weather_Station_2.ino` → `Weather_Station_2.cpp` (adding a manual `#include <Arduino.h>`) specifically because Arduino-CMake-Toolchain doesn't support `.ino` files. This is not just a toolchain quirk we're leaving behind — **arduino-cli itself requires the sketch's primary file to be named `<directory-name>.ino`**, confirmed empirically: pointing `arduino-cli compile` at a directory containing a correctly-named `.cpp` file fails with `Can't open sketch: main file missing from sketch: .../<name>.ino`, even though the same tool happily compiles other `.cpp`/`.h` files inside that sketch (e.g. everything under `src/`). So this change reverses that rename: `Weather_Station_2.cpp` → `Weather_Station_2.ino` (and likewise `tests/device/unit/DeviceUnitTests.cpp` → `.ino`, `tests/calibration/GrayscaleCalibration.cpp` → `.ino`). This isn't a step backward — arduino-cli is the tool that natively understands `.ino` preprocessing (implicit `Arduino.h`, auto-generated function prototypes), so the manual `#include <Arduino.h>` workaround added for the toolchain's sake can come back out.

2. **Board options as data, not files.** `cmake/BoardOptions.cmake` currently sets `ARDUINO_*_MENU_*` cache variables consumed by the toolchain. Under the new approach, board selection becomes `--fqbn esp32:esp32:Inkplate10` plus `--board-options PSRAM=enabled,PartitionScheme=huge_app,CPUFreq=240` passed directly to `arduino-cli compile`. Whether this lives as CMake variables in `cmake/BoardOptions.cmake` (for symmetry/discoverability) or as literal arguments in `CMakeLists.txt` is a planning-phase call — either preserves the "single source of truth, no interactive board menu" property the current file provides. `cmake/inkplate10-board.txt` (the custom board entry itself) is unaffected either way.

3. **Making generated headers visible to arduino-cli.** `Weather_Station_2.cpp` includes generated headers from `${CMAKE_BINARY_DIR}/generated/weather_station_2/*.h`, outside the sketch tree. arduino-cli needs an extra include path for that (likely a `--build-property` flag, e.g. an extra `-I` folded into `compiler.cpreprocessor.extra_flags` or similar) — needs to be nailed down and verified during planning/implementation, not guessed here.

4. **`compile_commands.json` generation.** `arduino-cli compile --only-compilation-database --build-path <dir>` emits it without a full compile; a real `compile` invocation should also produce/refresh it as a side effect. Need to confirm during planning whether CMake should invoke both (a fast DB-only step plus the real build) or rely on the real build alone.

5. **One custom target per existing Arduino executable.** Today there are three CMake-toolchain `add_executable` targets: `WeatherStation` (main firmware), `DeviceUnitTests` (`tests/device/unit/`), `GrayscaleCalibration` (`tests/calibration/`). Each becomes its own `add_custom_target` invoking `arduino-cli compile` against its own sketch directory with its own FQBN/board-options, mirroring the current per-target `CMakeLists.txt` structure. **`tests/unit/` (the host-side GTest project, `test-host` in `build.sh`) is a fully separate, plain-CMake project with no Arduino/toolchain dependency at all — entirely unaffected by this change.**

6. **Upload and monitor.** `target_enable_arduino_upload` today generates `upload-WeatherStation` etc. CMake targets. These get replaced with custom targets that shell out to `arduino-cli upload -p $SERIAL_PORT --fqbn ... --input-dir <build-path>`. `build.sh monitor` already calls `arduino-cli monitor` directly today and is unaffected.

7. **Partition table handling.** Today, `CMakeLists.txt`/per-target `CMakeLists.txt` files manually copy `huge_app.csv`/`default.csv` from `${ARDUINO_BOARD_RUNTIME_PLATFORM_PATH}/tools/partitions/` because the toolchain's post-link step expects it in the build directory. `arduino-cli compile --board-options PartitionScheme=huge_app` handles partition selection natively — this manual copy step should no longer be needed, but should be verified, not assumed.

8. **Incremental builds.** CMake can't practically enumerate every header a whole Arduino library tree depends on to decide whether to re-run the custom command. The plan should either always re-invoke `arduino-cli compile` (relying on arduino-cli's own internal build-cache in its `--build-path` to make repeat invocations fast) or use a coarse `DEPENDS` (e.g. glob `src/**` and the sketch file) accepting some over-triggering. This is a known, acceptable tradeoff, not a blocker.

9. **Containerfile simplification.** The `git clone .../Arduino-CMake-Toolchain` step and the nested-brace `platform.txt` sed patch both get removed. `arduino-cli`'s role expands from "one-time install" to "build driver," but it's already the pinned version (1.3.1) doing the installing, so no new dependency is introduced.

## Relationship to Issues #27 and #28

- **#27** (board core upgrade) is otherwise complete and can land independently of this work — the custom board entry and Containerfile core swap don't depend on which build driver consumes them. This document exists because implementing #27 surfaced that the *existing* build driver can't consume the result.
- **#28** (fork InkplateLibrary + submodule) remains necessary regardless of this change, and the InkplateLibrary source bugs surfaced by the spike (the NBSP character, the missed HTTPClient auto-detection) belong there, not here.
- This document's work should land **after** #27's board swap is confirmed working end-to-end with whatever build driver is in place at the time, so the two changes can be verified independently rather than debugged simultaneously.

## Out of Scope

- Fixing InkplateLibrary's own compilation issues against the newer core/compiler (NBSP character, any others the real build surfaces) — that's #28.
- Any change to the codegen mechanisms themselves (`Layout.cmake`, `Version.cmake`, `user_settings.h.in`) beyond ensuring their output is visible to arduino-cli's include path.
- Re-evaluating CMake as a build orchestrator entirely (e.g., replacing it with a plain shell script or Makefile) — out of scope; CMake's codegen role and single-entry-point role are working fine and aren't part of the problem.

## Open Questions for Planning

- Exact `--build-property`/flag needed to add `${CMAKE_BINARY_DIR}/generated` to arduino-cli's include path.
- Whether `--only-compilation-database` should run as a separate fast step or be folded into the main build invocation.
- Whether the manual partition-table copy step is truly obsolete under `--board-options PartitionScheme=...`, or still needed for the two non-default-partition targets (`WeatherStation` uses `huge_app`, `DeviceUnitTests`/`GrayscaleCalibration` use `default`).

## Acceptance Criteria (for the eventual plan)

- `cmake --build build` produces a flashable `.bin` for `WeatherStation`, `DeviceUnitTests`, and `GrayscaleCalibration`, driven by `arduino-cli compile` rather than `Arduino-CMake-Toolchain`.
- `build/compile_commands.json` (or equivalent) is generated and clangd/LSP still functions in Helix.
- `SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation` (or its equivalent under the new scheme) still flashes the device.
- `Containerfile` no longer clones `Arduino-CMake-Toolchain` and no longer carries the nested-brace `platform.txt` patch.
- `CLAUDE.md` build commands are updated to match whatever the final target names/invocations turn out to be.
- All existing codegen (`layout.h`, `version.h`, `user_settings.h`) continues to be generated and visible to the compiled firmware.
