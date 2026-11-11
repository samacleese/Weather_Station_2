# D3: OTA Sketch Integration and Button Confirmation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On each wake, check the OTA manifest. If newer firmware is published, offer it on screen. Install it only after a deliberate 2-second hold of the WAKE button and a battery check. Every other path falls through to the normal weather cycle.

**Architecture:** The boot splash moves from 3-bit to 1-bit mode, so every OTA screen draws onto it and refreshes with `partialUpdate()`. The display switches back to 3-bit just before the weather render. The two decisions that matter for safety (did someone really hold the button, and is the battery high enough) live in a pure-logic unit with host tests. The sketch does only the drawing and wiring.

**Tech Stack:** ESP32 Arduino (Soldered `soldered-inkplate-boards:esp32@3.0.0`), InkplateLibrary 11.1.2, GoogleTest host tests, CMake + `build.sh` in the `weather-station-builder` container.

**Spec:** GitHub issue #60 and the D3 section of `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md`. The design decisions Jesse approved on 2026-11-11 are listed below. Where they add to #60, they win.

## Design decisions (approved 2026-11-11)

1. **1-bit splash.** `Inkplate display(INKPLATE_1BIT)`. The splash and every OTA screen are 1-bit and refresh with `partialUpdate()`. `display.selectDisplayMode(INKPLATE_3BIT)` runs just before the existing `display.clearDisplay()` that starts the weather render.
2. **Static prompt.** There's no live countdown, so the poll loop never blocks on the display.
3. **Release before hold.** A hold only counts after WAKE has been seen released since the prompt appeared. A stuck button, or one still held from waking the device, can't confirm.
4. **5-second message hold.** The "battery too low" and "update failed" messages stay up for 5 s before the sketch falls through to the weather cycle.
5. **Throttled progress.** Download progress is redrawn only when the percentage has moved 10 points or more, plus once at 100%. Each partial update blocks the download while it draws.
6. **Person-caused wakes only.** The OTA check and prompt run only when `esp_sleep_get_wakeup_cause()` is `ESP_SLEEP_WAKEUP_EXT1` (WAKE button) or `ESP_SLEEP_WAKEUP_UNDEFINED` (reset/power-on). Timer wakes skip the manifest check. Added after the final branch review.

The manifest fetch is a single attempt with no retries.

## InkplateLibrary facts this plan relies on

Read from the build container: `/root/Arduino/libraries/InkplateLibrary/src/boards/Inkplate10/Inkplate10Driver.cpp`. The host may have stale copies, so always read library source from the container.

- `selectDisplayMode()` only sets a flag. `initializeFramebuffers()` allocates both the 1-bit and the 3-bit buffer in PSRAM no matter the mode, so switching at runtime is safe.
- `partialUpdate()` returns 0 without doing anything in 3-bit mode. In 1-bit mode, the first refresh after boot is a full `display1b`, and later calls are true partials. Every 10th partial is a forced full refresh (`_partialUpdateLimiter = 10`).
- **Colors differ by mode.** In 1-bit mode `BLACK` is 1 and `WHITE` is 0. In 3-bit mode black is 0 and white is 7. Use `BLACK`/`WHITE` while in 1-bit mode. The sketch's existing `setTextColor(0, 7)` would draw white text there.
- `display3b` runs its own clean cycles, so nothing from the 1-bit screen carries over into the weather render.

## Global Constraints

- Every new code file starts with two `// ABOUTME: ` lines.
- Format with `clang-format -i` (Google style, 120 columns). Match the surrounding style.
- Use `snprintf` and then `print`. `display.printf()` is not part of the Inkplate API.
- Format `uint64_t` with `PRIu64` from `<inttypes.h>`, never `%llu`. Don't use `%f` either. Print battery figures as integer millivolts.
- Font maps are `std::map`, and `.at()` on a missing size is a runtime failure. Sizes present in both `Roboto_Light` and `Roboto_Medium`: 12 14 17 20 24 29 35 42 50 60 72 87 104 125 (Medium also has 150).
- New layout fields need all four files: `layout.json`, `cmake/Layout.cmake`, `src/layout.h.in`, `tools/layout-editor/index.html`.
- WAKE button: `digitalRead(36) == LOW` means pressed. Use `pinMode(36, INPUT)`; the pin is input-only with an external pull-up.
- Commits are conventional commits referencing #60 and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never use `git add -A`; add the named files.
- Test output must be pristine.

---

## Setup (before Task 1)

- [ ] **Create the worktree and branch**

```bash
cd /home/jesse/Arduino/Weather_Station_2
git worktree add .worktrees/feat/issue-60-ota-confirmation -b feat/issue-60-ota-confirmation master
cp config.cmake .worktrees/feat/issue-60-ota-confirmation/config.cmake
cp docs/superpowers/plans/2026-11-11-d3-ota-sketch-integration.md .worktrees/feat/issue-60-ota-confirmation/docs/superpowers/plans/
cd .worktrees/feat/issue-60-ota-confirmation
./build.sh test-host
```

Expected: every existing test passes. If any fail, stop and fix them before going on.

- [ ] **Commit the plan on the branch**

```bash
git add docs/superpowers/plans/2026-11-11-d3-ota-sketch-integration.md
git commit -m "docs(ota): add D3 sketch integration plan (#60)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Then delete the untracked copy in the main checkout: `rm /home/jesse/Arduino/Weather_Station_2/docs/superpowers/plans/2026-11-11-d3-ota-sketch-integration.md`.

---

### Task 1: `UpdateConfirmation` hold detector

**Files:**
- Create: `src/ota/UpdateConfirmation.h`
- Create: `src/ota/UpdateConfirmation.cpp`
- Create: `tests/unit/UpdateConfirmationTest.cpp`
- Modify: `tests/unit/CMakeLists.txt` (add the test file and `../../src/ota/UpdateConfirmation.cpp` to `add_executable(unit_tests ...)`)

**Interfaces:**
- Produces:
  - `class UpdateConfirmation`
  - `enum Result { WAITING, CONFIRMED, TIMED_OUT }`
  - `Result sample(bool pressed, unsigned long nowMs)`
  - constants `HOLD_MS = 2000` and `WINDOW_MS = 30000`

- [ ] **Step 1: Write the failing tests**

`tests/unit/UpdateConfirmationTest.cpp`:

```cpp
// ABOUTME: Host unit tests for UpdateConfirmation's WAKE-button hold detection and the OTA battery gate.
// ABOUTME: Samples arrive at the sketch's 100 ms poll interval with explicit timestamps, so no real clock is needed.
#include <gtest/gtest.h>

#include "ota/UpdateConfirmation.h"

static const unsigned long POLL_MS = 100;

// Feeds the same button state every POLL_MS from startMs through endMs inclusive.
// Stops early and returns the first non-WAITING result; otherwise returns WAITING.
static UpdateConfirmation::Result feed(UpdateConfirmation& confirmation, bool pressed, unsigned long startMs,
                                       unsigned long endMs) {
    for (unsigned long t = startMs; t <= endMs; t += POLL_MS) {
        UpdateConfirmation::Result result = confirmation.sample(pressed, t);
        if (result != UpdateConfirmation::WAITING) return result;
    }
    return UpdateConfirmation::WAITING;
}

TEST(UpdateConfirmationTest, NoPressTimesOutAtThirtySeconds) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 0, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 30000));
}

TEST(UpdateConfirmationTest, TwoSecondHoldAfterReleaseConfirms) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 0));
    // Hold starts at 100 ms, so 2000 ms of hold is reached at 2100 ms.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 100, 2000));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 2100));
}

TEST(UpdateConfirmationTest, ReleaseRestartsTheHold) {
    UpdateConfirmation confirmation;
    confirmation.sample(false, 0);
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 100, 1900));
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 2000));
    // A new hold from 2100 ms needs until 4100 ms; the earlier 1.8 s does not carry over.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 2100, 4000));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 4100));
}

TEST(UpdateConfirmationTest, RepeatedShortPressesTimeOut) {
    UpdateConfirmation confirmation;
    // 500 ms pressed, 500 ms released, for the whole window.
    for (unsigned long t = 0; t < 30000; t += POLL_MS) {
        bool pressed = (t / 500) % 2 == 1;
        ASSERT_EQ(UpdateConfirmation::WAITING, confirmation.sample(pressed, t)) << "at " << t << " ms";
    }
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 30000));
}

TEST(UpdateConfirmationTest, HoldInProgressWhenPromptAppearsDoesNotCount) {
    UpdateConfirmation confirmation;
    // Still holding WAKE from waking the device: five seconds of LOW never confirms.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 0, 5000));
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 5100));
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 5200, 7100));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 7200));
}

TEST(UpdateConfirmationTest, StuckButtonTimesOut) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 0, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(true, 30000));
}

TEST(UpdateConfirmationTest, HoldUnfinishedAtWindowEndTimesOut) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 0, 28500));
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 28600, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(true, 30000));
}

TEST(UpdateConfirmationTest, WindowStartsAtFirstSample) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 50000, 79900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 80000));
}
```

Add `UpdateConfirmationTest.cpp` after `FirmwareInstallerTest.cpp`, and `../../src/ota/UpdateConfirmation.cpp` after `../../src/ota/FirmwareInstaller.cpp`, in `tests/unit/CMakeLists.txt`.

- [ ] **Step 2: Write a header-only stub and run the tests to see them fail**

Create `src/ota/UpdateConfirmation.h` with the final interface (Step 3), and a `.cpp` whose `sample()` returns `WAITING`, so the failures are assertion failures rather than link errors.

Run: `./build.sh test-host`
Expected: the `UpdateConfirmationTest` cases fail on `TIMED_OUT`/`CONFIRMED` expectations. All other suites still pass.

- [ ] **Step 3: Implement**

`src/ota/UpdateConfirmation.h`:

```cpp
// ABOUTME: Decides whether the person at the device confirmed an OTA install by holding WAKE for 2 seconds.
// ABOUTME: Pure logic fed with button samples and timestamps, so host tests cover every confirmation path.
#ifndef UPDATE_CONFIRMATION_H
#define UPDATE_CONFIRMATION_H

class UpdateConfirmation {
   public:
    enum Result { WAITING, CONFIRMED, TIMED_OUT };

    static const unsigned long HOLD_MS = 2000;
    static const unsigned long WINDOW_MS = 30000;

    // Feed one button sample. The 30 s window starts at the first sample. A hold only counts once the button
    // has been seen released, so a stuck button or one still held from waking the device can't confirm.
    Result sample(bool pressed, unsigned long nowMs);

   private:
    bool m_started = false;
    bool m_seenRelease = false;
    bool m_holding = false;
    unsigned long m_windowStartMs = 0;
    unsigned long m_holdStartMs = 0;
};

#endif  // UPDATE_CONFIRMATION_H
```

`src/ota/UpdateConfirmation.cpp`:

```cpp
// ABOUTME: Implements the WAKE-button hold detection used to confirm an OTA install.
// ABOUTME: See UpdateConfirmation.h for the confirmation rules.
#include "UpdateConfirmation.h"

UpdateConfirmation::Result UpdateConfirmation::sample(bool pressed, unsigned long nowMs) {
    if (!m_started) {
        m_started = true;
        m_windowStartMs = nowMs;
    }

    if (!pressed) {
        m_seenRelease = true;
        m_holding = false;
    } else if (m_seenRelease) {
        if (!m_holding) {
            m_holding = true;
            m_holdStartMs = nowMs;
        } else if (nowMs - m_holdStartMs >= HOLD_MS) {
            return CONFIRMED;
        }
    }

    if (nowMs - m_windowStartMs >= WINDOW_MS) return TIMED_OUT;
    return WAITING;
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `./build.sh test-host`
Expected: all tests pass, including 8 `UpdateConfirmationTest` cases. No warnings.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i src/ota/UpdateConfirmation.h src/ota/UpdateConfirmation.cpp tests/unit/UpdateConfirmationTest.cpp
./build.sh test-host
git add src/ota/UpdateConfirmation.h src/ota/UpdateConfirmation.cpp tests/unit/UpdateConfirmationTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat(ota): detect a deliberate 2 s WAKE hold to confirm an update (#60)

A hold only counts after the button has been seen released, so a stuck
button or one still held from waking the device cannot start a flash.
That rule is an addition to #60, agreed in D3 planning.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Battery gate

**Files:**
- Modify: `src/ota/UpdateConfirmation.h`, `src/ota/UpdateConfirmation.cpp`
- Test: `tests/unit/UpdateConfirmationTest.cpp`

**Interfaces:**
- Produces: `bool batteryAllowsInstall(float volts, int minBatteryMv)`, a free function declared in `UpdateConfirmation.h`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/UpdateConfirmationTest.cpp`:

```cpp
TEST(BatteryAllowsInstallTest, AllowsReadingExactlyAtThreshold) {
    // 3.6f is 3.5999999 in binary; a plain volts * 1000 >= mV comparison would wrongly refuse it.
    EXPECT_TRUE(batteryAllowsInstall(3.6f, 3600));
}

TEST(BatteryAllowsInstallTest, RefusesReadingBelowThreshold) { EXPECT_FALSE(batteryAllowsInstall(3.59f, 3600)); }

TEST(BatteryAllowsInstallTest, AllowsReadingAboveThreshold) { EXPECT_TRUE(batteryAllowsInstall(4.1f, 3600)); }

TEST(BatteryAllowsInstallTest, RefusesZeroReading) { EXPECT_FALSE(batteryAllowsInstall(0.0f, 3600)); }
```

Add the declaration to the header (Step 3) and a stub definition returning `false` in the `.cpp`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `./build.sh test-host`
Expected: `AllowsReadingExactlyAtThreshold` and `AllowsReadingAboveThreshold` fail.

- [ ] **Step 3: Implement**

In `UpdateConfirmation.h`, after the class and before `#endif`:

```cpp
// True when a battery reading in volts meets the manifest's min_battery_mv. The reading is rounded to whole
// millivolts first, so float error can't refuse a reading that is exactly at the threshold.
bool batteryAllowsInstall(float volts, int minBatteryMv);
```

In `UpdateConfirmation.cpp`, add `#include <cmath>` below the header include, and:

```cpp
bool batteryAllowsInstall(float volts, int minBatteryMv) { return std::lround(volts * 1000.0f) >= minBatteryMv; }
```

Update the `.h` ABOUTME second line to: `// ABOUTME: Also holds the battery gate. Both are pure logic, so host tests cover every confirmation path.`

- [ ] **Step 4: Run the tests to see them pass**

Run: `./build.sh test-host`
Expected: all pass, 12 cases across the two new suites.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i src/ota/UpdateConfirmation.h src/ota/UpdateConfirmation.cpp tests/unit/UpdateConfirmationTest.cpp
git add src/ota/UpdateConfirmation.h src/ota/UpdateConfirmation.cpp tests/unit/UpdateConfirmationTest.cpp
git commit -m "feat(ota): gate installs on battery voltage in whole millivolts (#60)

Rounding first keeps 3.6f (3.5999999 V) from failing a 3600 mV threshold.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 1-bit splash

**Files:**
- Modify: `Weather_Station_2.ino:48` (display construction), `:100` (splash text color), `:119` (mode switch)

**Interfaces:**
- Produces: the display is in `INKPLATE_1BIT` from boot until just before the weather render, and the splash has been fully refreshed once, so `partialUpdate()` works from then on. Task 5 relies on this.

This is sketch-level Inkplate code with no host-test seam (no existing sketch code has one). It's verified on hardware.

- [ ] **Step 1: Change the display mode**

Replace line 48 with:

```cpp
// Boots in 1-bit so the splash and OTA screens can use partialUpdate(), which 3-bit mode does not support.
// setup() switches to 3-bit before drawing the weather screen.
Inkplate display(INKPLATE_1BIT);
```

- [ ] **Step 2: Fix the splash text color**

In the splash block, replace `display.setTextColor(0, 7);` with:

```cpp
    display.setTextColor(BLACK, WHITE);  // 1-bit colors; 0 would be white in this mode
```

- [ ] **Step 3: Switch to 3-bit before the weather render**

Replace `display.clearDisplay();` right after `int updateResult = curr.update(3);` with:

```cpp
    display.selectDisplayMode(INKPLATE_3BIT);
    display.clearDisplay();
```

Both the error render and the normal render follow this line, so both draw in 3-bit.

- [ ] **Step 4: Build, flash and check on hardware**

```bash
./build.sh configure && ./build.sh build && ./build.sh flash
python3 tools/serial_capture.py --seconds 40
```

Expected serial: the boot output from `CLAUDE.md` (WiFi connected, NTP, `[HTTPS] GET... code: 200`) with no new errors.

Ask Jesse to look at the panel: the splash should be black text on white, as before, and the weather screen should render normally with the kitty in grayscale. If the splash shows white-on-black or nothing, the color constants are wrong. Stop and debug.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i Weather_Station_2.ino
git diff --stat   # only Weather_Station_2.ino; if clang-format touched unrelated lines, revert those hunks
git add Weather_Station_2.ino
git commit -m "feat(display): draw the boot splash in 1-bit mode (#60)

partialUpdate() only works in 1-bit mode, and D3's OTA prompt and progress
screens draw on top of the splash. The weather screen still renders in
3-bit; the mode switches just before it. Both framebuffers are always
allocated by InkplateLibrary 11.1.2, so the runtime switch is safe.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: OTA layout fields

**Files:**
- Modify: `layout.json`, `cmake/Layout.cmake`, `src/layout.h.in`, `tools/layout-editor/index.html`

**Interfaces:**
- Produces:
  - `LAYOUT_LOADING_OTA_PROMPT_X`, `LAYOUT_LOADING_OTA_PROMPT_Y`, `LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE` (Roboto_Medium)
  - `LAYOUT_LOADING_OTA_STATUS_X`, `LAYOUT_LOADING_OTA_STATUS_Y`, `LAYOUT_LOADING_OTA_STATUS_FONT_SIZE` (Roboto_Light)

- [ ] **Step 1: `layout.json`**

Inside `"loading"`, after `"version"` (add the comma):

```json
      "version":      { "x": 300, "y": 395, "font_size": 14  },
      "ota_prompt":   { "x": 300, "y": 470, "font_size": 29  },
      "ota_status":   { "x": 300, "y": 530, "font_size": 24  }
```

- [ ] **Step 2: `cmake/Layout.cmake`**

After the "Loading screen — version" block, before `configure_file`:

```cmake
# Loading screen — OTA update prompt
string(JSON LAYOUT_LOADING_OTA_PROMPT_X         GET ${LAYOUT_JSON} screens loading ota_prompt x)
string(JSON LAYOUT_LOADING_OTA_PROMPT_Y         GET ${LAYOUT_JSON} screens loading ota_prompt y)
string(JSON LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE GET ${LAYOUT_JSON} screens loading ota_prompt font_size)

# Loading screen — OTA status line (battery, progress, result)
string(JSON LAYOUT_LOADING_OTA_STATUS_X         GET ${LAYOUT_JSON} screens loading ota_status x)
string(JSON LAYOUT_LOADING_OTA_STATUS_Y         GET ${LAYOUT_JSON} screens loading ota_status y)
string(JSON LAYOUT_LOADING_OTA_STATUS_FONT_SIZE GET ${LAYOUT_JSON} screens loading ota_status font_size)
```

- [ ] **Step 3: `src/layout.h.in`**

Append:

```c
// Loading screen — OTA update prompt
#define LAYOUT_LOADING_OTA_PROMPT_X         @LAYOUT_LOADING_OTA_PROMPT_X@
#define LAYOUT_LOADING_OTA_PROMPT_Y         @LAYOUT_LOADING_OTA_PROMPT_Y@
#define LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE @LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE@

// Loading screen — OTA status line (battery, progress, result)
#define LAYOUT_LOADING_OTA_STATUS_X         @LAYOUT_LOADING_OTA_STATUS_X@
#define LAYOUT_LOADING_OTA_STATUS_Y         @LAYOUT_LOADING_OTA_STATUS_Y@
#define LAYOUT_LOADING_OTA_STATUS_FONT_SIZE @LAYOUT_LOADING_OTA_STATUS_FONT_SIZE@
```

- [ ] **Step 4: `tools/layout-editor/index.html`**

The editor builds its controls from `Object.entries(screen)`, so only the four lookup tables need entries:

- `SAMPLE_TEXT`: add `ota_prompt: 'Update 20261111001 ready. Hold WAKE 2s to install.',` and `ota_status: 'Battery 82%. Skips in 30s if not held.',`
- `IS_BOLD.loading`: add `ota_prompt: true,` and `ota_status: false,`
- `FONT_WEIGHT_LABEL.loading`: add `ota_prompt: 'Roboto_Medium',` and `ota_status: 'Roboto_Light',`
- `DEFAULT_LAYOUT.screens.loading`: add `ota_prompt: { x: 300, y: 470, font_size: 29 },` and `ota_status: { x: 300, y: 530, font_size: 24 },`

- [ ] **Step 5: Verify**

```bash
./build.sh configure
grep -n "LAYOUT_LOADING_OTA" build/generated/weather_station_2/layout.h
```

Expected: six `#define` lines with the values above. If the generated header lives elsewhere, find it with `find build -name layout.h`.

Open `tools/layout-editor/index.html` in a browser, choose the Loading screen, and check that both new elements render and neither line runs past the 1200 px edge. If the prompt is too wide, drop it to 24 in all four files.

- [ ] **Step 6: Commit**

```bash
git add layout.json cmake/Layout.cmake src/layout.h.in tools/layout-editor/index.html
git commit -m "feat(layout): add OTA prompt and status lines to the loading screen (#60)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Wire the OTA offer into the wake cycle

**Files:**
- Modify: `Weather_Station_2.ino`

**Interfaces:**
- Consumes:
  - `UpdateConfirmation` and `batteryAllowsInstall` (Tasks 1-2)
  - the `LAYOUT_LOADING_OTA_*` defines (Task 4)
  - the 1-bit splash (Task 3)
  - `OTAManager(IHttpClient&, uint64_t)` and `bool checkForUpdate(const char*, UpdateInfo&)`
  - `FirmwareInstaller(IFirmwareSource&, IOtaPartition&, IImageVerifier&)` and `bool install(const UpdateInfo&, ProgressFn)`
  - `HttpFirmwareSource`, `EspOtaPartition`, `EcdsaImageVerifier` (all default-constructible)
  - `IBatteryReader::readVoltage() const` and `BatteryMeter::voltageToPercent(float)`
- Produces: `static void offerFirmwareUpdate(IHttpClient& http, const IBatteryReader& batteryReader)`, called once after `network->begin()`

- [ ] **Step 1: Includes and constants**

Add `#include <inttypes.h>` after `#include <rom/rtc.h>`. After `#include "src/display/InkplateBatteryReader.h"` add:

```cpp
#include "src/ota/EcdsaImageVerifier.h"
#include "src/ota/EspOtaPartition.h"
#include "src/ota/FirmwareInstaller.h"
#include "src/ota/HttpFirmwareSource.h"
#include "src/ota/OTAManager.h"
#include "src/ota/UpdateConfirmation.h"
```

After `static const uint16_t INK_BLACK = 0;` add:

```cpp
static const uint8_t WAKE_BUTTON_PIN = 36;
static const unsigned long BUTTON_POLL_MS = 100;
// Long enough to read a refusal or failure before the weather screen replaces it.
static const unsigned long OTA_MESSAGE_HOLD_MS = 5000;
static const int PROGRESS_STEP_PERCENT = 10;
```

- [ ] **Step 2: Drawing helpers**

After `drawPrecipForecast`:

```cpp
// Draws one line of OTA text on the 1-bit splash without refreshing the panel. Clears the line's band first:
// yAdvance above the baseline covers ascenders, and a third of it below covers descenders.
static void drawOtaLine(const GFXfont* font, int16_t x, int16_t y, const char* text) {
    display.fillRect(0, y - font->yAdvance, display.width(), font->yAdvance + font->yAdvance / 3, WHITE);
    display.setFont(font);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(x, y);
    display.print(text);
}

// Refreshes only the changed pixels. Logged because each refresh blocks the download it reports on.
static void refreshOtaScreen() {
    unsigned long startMs = millis();
    display.partialUpdate();
    Log.notice(F("OTA screen refresh took %lu ms" CR), millis() - startMs);
}

static void showOtaStatus(const char* text) {
    drawOtaLine(Roboto_Light.at(LAYOUT_LOADING_OTA_STATUS_FONT_SIZE), LAYOUT_LOADING_OTA_STATUS_X,
                LAYOUT_LOADING_OTA_STATUS_Y, text);
    refreshOtaScreen();
}

static int lastShownProgressPercent = 0;

// FirmwareInstaller progress callback: redraws only every PROGRESS_STEP_PERCENT points and at the end.
static void showDownloadProgress(size_t received, size_t total) {
    if (total == 0) return;
    int percent = (int)((uint64_t)received * 100 / total);
    if (received < total && percent < lastShownProgressPercent + PROGRESS_STEP_PERCENT) return;
    lastShownProgressPercent = percent;
    snprintf(buffer, sizeof(buffer), "Downloading %d%%... Do not power off", percent);
    showOtaStatus(buffer);
}
```

- [ ] **Step 3: The offer flow**

After the helpers:

```cpp
// Offers newer firmware when the manifest has one. Returns when the weather cycle should carry on; a
// successful install restarts the device instead of returning.
static void offerFirmwareUpdate(IHttpClient& http, const IBatteryReader& batteryReader) {
    OTAManager otaManager(http, COMPILED_VERSION);
    OTAManager::UpdateInfo info;
    if (!otaManager.checkForUpdate(OTA_MANIFEST_URL, info)) return;

    snprintf(buffer, sizeof(buffer), "Update %" PRIu64 " ready. Hold WAKE 2s to install.", info.version);
    Log.notice(F("%s" CR), buffer);
    drawOtaLine(Roboto_Medium.at(LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE), LAYOUT_LOADING_OTA_PROMPT_X,
                LAYOUT_LOADING_OTA_PROMPT_Y, buffer);
    snprintf(buffer, sizeof(buffer), "Battery %d%%. Skips in 30s if not held.",
             BatteryMeter::voltageToPercent(batteryReader.readVoltage()));
    showOtaStatus(buffer);

    pinMode(WAKE_BUTTON_PIN, INPUT);  // input-only pin with an external pull-up on the Inkplate 10
    UpdateConfirmation confirmation;
    UpdateConfirmation::Result result;
    while ((result = confirmation.sample(digitalRead(WAKE_BUTTON_PIN) == LOW, millis())) ==
           UpdateConfirmation::WAITING) {
        delay(BUTTON_POLL_MS);
    }
    if (result == UpdateConfirmation::TIMED_OUT) {
        Log.notice(F("OTA update skipped: WAKE was not held" CR));
        return;
    }

    // This reading, not the one on the prompt, is the one that protects the flash.
    float voltage = batteryReader.readVoltage();
    if (!batteryAllowsInstall(voltage, info.min_battery_mv)) {
        snprintf(buffer, sizeof(buffer), "Battery too low to update (%ld mV, needs %d mV)",
                 std::lround(voltage * 1000.0f), info.min_battery_mv);
        Log.warning(F("%s" CR), buffer);
        showOtaStatus(buffer);
        delay(OTA_MESSAGE_HOLD_MS);
        return;
    }

    // Static keeps HTTPClient and the mbedtls context off the 8KB loop-task stack.
    static HttpFirmwareSource source;
    static EspOtaPartition partition;
    static EcdsaImageVerifier verifier;
    FirmwareInstaller installer(source, partition, verifier);

    lastShownProgressPercent = 0;
    showOtaStatus("Downloading 0%... Do not power off");
    if (installer.install(info, showDownloadProgress)) {
        Log.notice(F("OTA update installed, restarting" CR));
        showOtaStatus("Update installed. Rebooting...");
        esp_restart();
    }

    Log.error(F("OTA install failed" CR));
    showOtaStatus("Update failed. Continuing to weather.");
    delay(OTA_MESSAGE_HOLD_MS);
}
```

`std::lround` needs `#include <cmath>`. Add it next to `#include <memory>`.

- [ ] **Step 4: Call it, and stop the error render inheriting the OTA font**

In `setup()`, right after `network->begin();`:

```cpp
    offerFirmwareUpdate(*network, batteryReader);
```

The error render (`if (updateResult != CURRENT_CONDITIONS_OK)`) never sets a font, so it prints in whatever font was last selected. Until now that was the splash's version font (Roboto_Light 14). After an OTA fall-through it would be the status font instead. Make it explicit. In that block, after `display.setTextColor(0, 7);`:

```cpp
        display.setFont(Roboto_Light.at(14));
```

- [ ] **Step 5: Build and check size**

```bash
clang-format -i Weather_Station_2.ino
./build.sh build
```

Expected: the build succeeds and the size budget check (#53) passes. Write the reported firmware size in the PR. `checkForUpdate` and the installer were already compiled in through `src/`, so growth should be small.

- [ ] **Step 6: Commit**

```bash
git add Weather_Station_2.ino
git commit -m "feat(ota): offer published firmware on wake behind a 2 s WAKE hold (#60)

Checks the manifest after WiFi comes up. When newer firmware is published it
shows a prompt on the 1-bit splash, waits up to 30 s for a deliberate hold,
re-reads the battery against min_battery_mv, then installs and restarts.
Refusals and failures stay on screen for 5 s and fall through to the
weather cycle. Progress redraws every 10 points because each partial
update blocks the download.

The error render now sets its own font; it used to inherit the splash's.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Hardware acceptance (#60 criteria)

**Files:** none changed, unless a case fails. A failure starts a systematic-debugging pass on the cause, not a workaround.

Jesse has to be at the device for the button cases. Ask Jesse for the Pi host and the private key path. Don't guess them. The spec's example is `keys/ecdsa_private.pem` and `192.168.1.2:5000`.

One forced full-screen flash during the download is expected (InkplateLibrary forces a full refresh every 10th partial update). It is not a bug.

- [ ] **Step 1: Baseline.** Flash this branch: `./build.sh flash`. Check what's published with `curl -s http://<pi-host>/ota-firmware/data`. If its `version` is above the contents of `VERSION`, re-publish an older image or note that the "no update" case runs after Step 4.
- [ ] **Step 2: No update available.** `python3 tools/serial_capture.py --seconds 60`. Expected: no `Update ... ready` log line, no prompt on the panel, a normal weather screen.
- [ ] **Step 3: Publish a newer build.** Temporarily raise `VERSION` (e.g. `20261111001`), then `./build.sh configure && ./build.sh build`, then `python3 tools/ota_publish.py --firmware build/WeatherStation/Weather_Station_2.ino.bin --private-key <key> --pi-host <pi-host>`. Then `git checkout VERSION && ./build.sh configure && ./build.sh build && ./build.sh flash` so the device runs the lower version again.
- [ ] **Step 4: No hold.** Capture 90 s and don't touch the device. Expected: the prompt and battery line appear, `OTA update skipped: WAKE was not held` is logged about 30 s later, then a normal weather screen. That also shows WiFi survived the window. Record the `OTA screen refresh took N ms` figure.
- [ ] **Step 5: Short presses only.** Same as Step 4, with several quick taps during the window. Expected: skipped after 30 s.
- [ ] **Step 6: Held through boot.** Hold WAKE, tap RESET, and keep holding WAKE for about 10 s after the prompt appears, then let go and don't touch it again. Expected: no install; skipped at 30 s.
- [ ] **Step 7: Low battery.** Re-publish the same newer build with `--min-battery-mv 5000`, reflash the lower version, hold WAKE for 2 s at the prompt. Expected: `Battery too low to update (NNNN mV, needs 5000 mV)` on screen for about 5 s, then the weather screen.
- [ ] **Step 8: Install failure.** Re-publish with the default threshold, then overwrite only the binary with a different build (e.g. this branch's build without the VERSION bump): `curl -X PUT --data-binary @build/WeatherStation/Weather_Station_2.ino.bin http://<pi-host>/ota-firmware/binaries/firmware`. Check the endpoint against the json-store or `tools/ota_publish.py` before running it. Hold 2 s. Expected: the download progresses, then `Update failed. Continuing to weather.` for about 5 s, a normal weather screen, and the boot partition unchanged on the next boot.
- [ ] **Step 9: Successful install.** Publish the newer build normally, hold WAKE at about 25 s into the window. Expected: progress at about 10% steps, `Update installed. Rebooting...`, a reboot, a `Firmware version:` line showing the new build, and on that boot no prompt (versions equal). Record total install time and how many refreshes ran.
- [ ] **Step 10: Real deep-sleep wake, confirmed.** With the newer build published, let the device finish a cycle and go to deep sleep. Press WAKE to wake it, release, then hold WAKE 2 s at the prompt. Expected: it confirms (proves GPIO 36 reads correctly after an ext1 wake). Stop at the download if Step 9 has already run, or let it install.
- [ ] **Step 11: Timer wake skips the check.** With a newer build published, leave the device alone through one 10-minute timer wake. Expected: no `Update ... ready` line in serial, no prompt, normal weather screen without the 30 s delay.
- [ ] **Step 12: Restore.** `./build.sh flash` with the committed `VERSION` so the device isn't left on a test build. Leave the Pi manifest at a version that doesn't prompt on every wake. Ask Jesse how to leave it.

If the measured refresh time makes the download noticeably slower (for example, refreshes adding more than half of the roughly 8 s stream time), tell Jesse before changing `PROGRESS_STEP_PERCENT`.

---

### Task 7: Docs and PR

**Files:**
- Modify: `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md` (D3 row to ✅, D3 section results)
- Modify: `CLAUDE.md` (Architecture flow line)

- [ ] **Step 1: Roadmap.** In the story index, change the D3 row's ❌ to ✅. Under `### D3. Sketch integration`, add a "Done" paragraph covering:
  - the 1-bit splash and its library facts (both buffers allocated, partials every 10th forced full, BLACK=1)
  - the release-before-hold rule
  - the 5 s message hold
  - the 10-point progress throttle
  - the measured refresh time and install time from Task 6
  - the final firmware size
- [ ] **Step 2: `CLAUDE.md`.** In the Architecture flow, change `init display → WiFi → NTP → fetch weather` to `init display (1-bit splash) → WiFi → NTP → OTA check (prompt + 2 s WAKE hold if newer firmware is published) → fetch weather (3-bit from here)`.
- [ ] **Step 3: Commit, push, PR**

```bash
git add docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md CLAUDE.md
git commit -m "docs(ota): record D3 results and the 1-bit splash (#60)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Push over HTTPS (SSH push hangs from the Claude shell; see the memory note) and open a PR against `master` titled `feat(ota): D3 sketch integration and WAKE-hold confirmation (#60)`. In the body:
- link #60
- list the Task 6 results case by case
- call out the two additions to #60: release-before-hold, and the 5 s message hold
- end with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`

Ask Jesse before merging.

---

## Out of scope

- The full negative matrix (tampered SHA-256/signature, missing manifest field, mid-stream WiFi drop): D4 #61.
- Deleting the throwaway spikes in `tests/spikes/`: D4 still uses `OtaInstall` for its negative cases.
- Trickled response headers blocking `HTTPClient::GET()`: #82.
