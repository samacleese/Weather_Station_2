# Battery Meter Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the broken battery voltage display (ADC_OFFSET=-0.50 was wrong) with an empirical 0–100% SOC meter backed by a testable `IBatteryReader`/`BatteryMeter` abstraction.

**Architecture:** A new `IBatteryReader` interface separates hardware from logic; `InkplateBatteryReader` wraps `display.readBattery()`; `BatteryMeter` does pure-math voltage→% lookup (testable on host). `BatteryLogger` is simplified to log raw voltage only. Both call sites in `Weather_Station_2.cpp` are updated.

**Tech Stack:** C++11, Arduino/ESP32, GoogleTest (host unit tests), CMake, `./build.sh test-host`

**Spec:** `docs/superpowers/specs/2026-05-29-battery-meter-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `src/display/IBatteryReader.h` | Create | Abstract interface for reading battery voltage |
| `src/display/BatteryMeter.h` | Create | Static utility: voltage → SOC% |
| `src/display/BatteryMeter.cpp` | Create | Empirical lookup table + piecewise linear interpolation |
| `src/display/InkplateBatteryReader.h` | Create | Hardware adapter header |
| `src/display/InkplateBatteryReader.cpp` | Create | Hardware adapter impl (wraps `display.readBattery()`) |
| `src/network/BatteryLogger.h` | Modify | Remove `adjustedVoltage` param from `log()` |
| `src/network/BatteryLogger.cpp` | Modify | Log `{ts, raw}` only; update ABOUTME |
| `Weather_Station_2.cpp` | Modify | Remove ADC_OFFSET; add includes; wire reader + meter; update both display call sites; update BatteryLogger call |
| `tests/unit/BatteryMeterTest.cpp` | Create | Unit tests for `BatteryMeter::voltageToPercent` |
| `tests/unit/CMakeLists.txt` | Modify | Add `BatteryMeterTest.cpp` and `BatteryMeter.cpp` to test target |

---

## Chunk 1: Display components — IBatteryReader, BatteryMeter, InkplateBatteryReader

### Task 1: Create `IBatteryReader` interface

**Files:**
- Create: `src/display/IBatteryReader.h`

- [ ] **Step 1: Create the header**

```cpp
// ABOUTME: Abstract interface for reading battery voltage from hardware.
// ABOUTME: Enables hardware-independent testing of battery-dependent code.
#ifndef IBATTERY_READER_H
#define IBATTERY_READER_H

class IBatteryReader {
public:
    virtual ~IBatteryReader() = default;
    virtual float readVoltage() const = 0;
};

#endif  // IBATTERY_READER_H
```

- [ ] **Step 2: Verify it compiles (build firmware)**

```bash
./build.sh build
```

Expected: build succeeds (file is not included anywhere yet, so no effect).

- [ ] **Step 3: Commit**

```bash
git add src/display/IBatteryReader.h
git commit -m "feat: add IBatteryReader interface"
```

---

### Task 2: `BatteryMeter` — tests first, then implementation

**Files:**
- Create: `tests/unit/BatteryMeterTest.cpp`
- Create: `src/display/BatteryMeter.h`
- Create: `src/display/BatteryMeter.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Add `BatteryMeterTest.cpp` and `BatteryMeter.cpp` to the test CMakeLists**

In `tests/unit/CMakeLists.txt`, expand `add_executable` from:
```cmake
add_executable(unit_tests
    CACertsTest.cpp
    CurrentConditionsTest.cpp
    ../../src/security/CACerts.cpp
    ../../src/network/CurrentConditions.cpp
)
```
To:
```cmake
add_executable(unit_tests
    CACertsTest.cpp
    CurrentConditionsTest.cpp
    BatteryMeterTest.cpp
    ../../src/security/CACerts.cpp
    ../../src/network/CurrentConditions.cpp
    ../../src/display/BatteryMeter.cpp
)
```

Do **not** add `InkplateBatteryReader.cpp` — it depends on `Inkplate.h` which is unavailable in the host build.

- [ ] **Step 2: Write the failing tests**

Create `tests/unit/BatteryMeterTest.cpp`:

```cpp
// ABOUTME: Unit tests for BatteryMeter::voltageToPercent.
// ABOUTME: Covers all 11 breakpoints, midpoint interpolation, and clamping.
#include <gtest/gtest.h>
#include "display/BatteryMeter.h"

// --- Breakpoints ---
TEST(BatteryMeterTest, Breakpoint100) { EXPECT_EQ(BatteryMeter::voltageToPercent(4.294f), 100); }
TEST(BatteryMeterTest, Breakpoint90)  { EXPECT_EQ(BatteryMeter::voltageToPercent(4.188f),  90); }
TEST(BatteryMeterTest, Breakpoint80)  { EXPECT_EQ(BatteryMeter::voltageToPercent(4.118f),  80); }
TEST(BatteryMeterTest, Breakpoint70)  { EXPECT_EQ(BatteryMeter::voltageToPercent(4.096f),  70); }
TEST(BatteryMeterTest, Breakpoint60)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.994f),  60); }
TEST(BatteryMeterTest, Breakpoint50)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.954f),  50); }
TEST(BatteryMeterTest, Breakpoint40)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.890f),  40); }
TEST(BatteryMeterTest, Breakpoint30)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.836f),  30); }
TEST(BatteryMeterTest, Breakpoint20)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.704f),  20); }
TEST(BatteryMeterTest, Breakpoint10)  { EXPECT_EQ(BatteryMeter::voltageToPercent(3.578f),  10); }
TEST(BatteryMeterTest, Breakpoint0)   { EXPECT_EQ(BatteryMeter::voltageToPercent(3.224f),   0); }

// --- Interpolation ---
// Midpoint between 4.188V (90%) and 4.294V (100%) = 4.241V → 95%
TEST(BatteryMeterTest, MidpointInterpolation) {
    EXPECT_EQ(BatteryMeter::voltageToPercent(4.241f), 95);
}

// --- Clamping ---
TEST(BatteryMeterTest, ClampBelow) { EXPECT_EQ(BatteryMeter::voltageToPercent(3.0f),  0); }
TEST(BatteryMeterTest, ClampAbove) { EXPECT_EQ(BatteryMeter::voltageToPercent(4.5f), 100); }
```

- [ ] **Step 3: Create the stub header so the test compiles**

Create `src/display/BatteryMeter.h`:

```cpp
// ABOUTME: Converts battery voltage to state-of-charge percentage.
// ABOUTME: Uses an empirical lookup table derived from a full discharge cycle.
#ifndef BATTERY_METER_H
#define BATTERY_METER_H

class BatteryMeter {
public:
    static int voltageToPercent(float voltage);
};

#endif  // BATTERY_METER_H
```

- [ ] **Step 4: Create a stub `.cpp` so it links**

Create `src/display/BatteryMeter.cpp` with just a stub:

```cpp
// ABOUTME: Converts battery voltage to state-of-charge percentage.
// ABOUTME: Uses an empirical lookup table derived from a full discharge cycle.
#include "BatteryMeter.h"

int BatteryMeter::voltageToPercent(float /*voltage*/) {
    return -1;  // stub
}
```

- [ ] **Step 5: Run tests and confirm they fail**

```bash
./build.sh test-host
```

Expected: all `BatteryMeterTest` cases FAIL (returning -1, not the expected values). Existing `CACertsTest` and `CurrentConditionsTest` must still pass.

- [ ] **Step 6: Implement `BatteryMeter::voltageToPercent`**

Replace `src/display/BatteryMeter.cpp` with the full implementation:

```cpp
// ABOUTME: Converts battery voltage to state-of-charge percentage.
// ABOUTME: Uses an empirical lookup table derived from a full discharge cycle.
#include "BatteryMeter.h"

#include <cmath>

namespace {

struct VoltagePoint {
    float voltage;
    int percent;
};

static const VoltagePoint kTable[] = {
    {4.294f, 100},
    {4.188f,  90},
    {4.118f,  80},
    {4.096f,  70},
    {3.994f,  60},
    {3.954f,  50},
    {3.890f,  40},
    {3.836f,  30},
    {3.704f,  20},
    {3.578f,  10},
    {3.224f,   0},
};

static const int kTableSize = static_cast<int>(sizeof(kTable) / sizeof(kTable[0]));

}  // namespace

int BatteryMeter::voltageToPercent(float voltage) {
    if (voltage >= kTable[0].voltage) return 100;
    if (voltage <= kTable[kTableSize - 1].voltage) return 0;

    for (int i = 0; i < kTableSize - 1; ++i) {
        if (kTable[i].voltage >= voltage && kTable[i + 1].voltage < voltage) {
            if (voltage == kTable[i].voltage) return kTable[i].percent;
            float ratio = (voltage - kTable[i + 1].voltage) /
                          (kTable[i].voltage - kTable[i + 1].voltage);
            float result = static_cast<float>(kTable[i + 1].percent) +
                           ratio * static_cast<float>(kTable[i].percent - kTable[i + 1].percent);
            return static_cast<int>(std::round(result));
        }
    }
    return 0;  // unreachable: all in-range voltages are covered by the loop
}
```

- [ ] **Step 7: Run tests and confirm they all pass**

```bash
./build.sh test-host
```

Expected: all 14 `BatteryMeterTest` cases PASS. All existing tests still pass. Output ends with something like:
```
[  PASSED  ] 14 tests.
```

- [ ] **Step 8: Commit**

```bash
git add src/display/BatteryMeter.h src/display/BatteryMeter.cpp \
        tests/unit/BatteryMeterTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: add BatteryMeter voltage-to-percent lookup

Empirical 11-point table from 43-day discharge cycle.
Piecewise linear interpolation with std::round."
```

---

### Task 3: `InkplateBatteryReader` hardware adapter

**Files:**
- Create: `src/display/InkplateBatteryReader.h`
- Create: `src/display/InkplateBatteryReader.cpp`

No unit tests — this is a one-line hardware wrapper, verified by flashing.

- [ ] **Step 1: Create the header**

Create `src/display/InkplateBatteryReader.h`:

```cpp
// ABOUTME: Reads battery voltage from Inkplate hardware via display.readBattery().
// ABOUTME: Implements IBatteryReader for use in production firmware.
#ifndef INKPLATE_BATTERY_READER_H
#define INKPLATE_BATTERY_READER_H

#include "IBatteryReader.h"
#include <Inkplate.h>

class InkplateBatteryReader : public IBatteryReader {
public:
    explicit InkplateBatteryReader(Inkplate& display);
    float readVoltage() const override;

private:
    Inkplate& m_display;
};

#endif  // INKPLATE_BATTERY_READER_H
```

- [ ] **Step 2: Create the implementation**

Create `src/display/InkplateBatteryReader.cpp`:

```cpp
// ABOUTME: Reads battery voltage from Inkplate hardware via display.readBattery().
// ABOUTME: Implements IBatteryReader for use in production firmware.
#include "InkplateBatteryReader.h"

InkplateBatteryReader::InkplateBatteryReader(Inkplate& display) : m_display(display) {}

float InkplateBatteryReader::readVoltage() const {
    return m_display.readBattery();
}
```

- [ ] **Step 3: Verify firmware still builds**

```bash
./build.sh build
```

Expected: build succeeds (files exist but are not wired in yet).

- [ ] **Step 4: Commit**

```bash
git add src/display/InkplateBatteryReader.h src/display/InkplateBatteryReader.cpp
git commit -m "feat: add InkplateBatteryReader hardware adapter"
```

---

## Chunk 2: BatteryLogger simplification + Weather_Station_2.cpp wiring

> **Prerequisite:** Chunk 1 must be complete. `src/display/IBatteryReader.h`, `BatteryMeter.h/.cpp`, and `InkplateBatteryReader.h/.cpp` must already exist.

### Task 4: Simplify `BatteryLogger`

**Files:**
- Modify: `src/network/BatteryLogger.h`
- Modify: `src/network/BatteryLogger.cpp`

- [ ] **Step 1: Update the header**

In `src/network/BatteryLogger.h`, change the `log` declaration from:
```cpp
void log(time_t timestamp, float rawVoltage, float adjustedVoltage);
```
to:
```cpp
void log(time_t timestamp, float voltage);
```

- [ ] **Step 2: Update the implementation**

Replace `src/network/BatteryLogger.cpp` with:

```cpp
// ABOUTME: Posts battery voltage readings to a local JSON store for discharge curve analysis.
// ABOUTME: Sends a single {ts, raw} entry per reading; the server appends to the array.
#include "BatteryLogger.h"

#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <HTTPClient.h>

BatteryLogger::BatteryLogger(const String& url) : m_url(url) {}

void BatteryLogger::log(time_t timestamp, float voltage) {
    StaticJsonDocument<64> doc;
    doc["ts"] = (long)timestamp;
    doc["raw"] = voltage;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(m_url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);

    if (httpCode != HTTP_CODE_CREATED) {
        Log.warning(F("[BatteryLogger] POST failed: %d" CR), httpCode);
    }

    http.end();
}
```

- [ ] **Step 3: Verify firmware build fails with the expected error**

```bash
./build.sh build
```

Expected: compile error on the `batteryLogger.log(now, rawBattery, voltage)` call site in `Weather_Station_2.cpp` — too many arguments. This confirms the signature change is in effect.

- [ ] **Step 4: Run host tests to confirm nothing broke there**

```bash
./build.sh test-host
```

Expected: all tests pass (BatteryLogger is not used by host tests).

- [ ] **Step 5: Commit**

```bash
git add src/network/BatteryLogger.h src/network/BatteryLogger.cpp
git commit -m "feat: simplify BatteryLogger to log raw voltage only

Remove adjustedVoltage param — ADC offset is now zero so adj was
redundant. New payload: {ts, raw}."
```

---

### Task 5: Wire `Weather_Station_2.cpp`

**Files:**
- Modify: `Weather_Station_2.cpp`

The firmware currently fails to build (broken `batteryLogger.log` call from Task 4). This task fixes it and completes the feature.

There are two `readBattery()` call sites. The **error path** is inside
`if (updateResult != CURRENT_CONDITIONS_OK)` → inner `else` block (~line 113);
the **normal path** is around line 194. Work top-to-bottom.

- [ ] **Step 1: Add includes**

After the existing includes (around line 34), add:

```cpp
#include "src/display/BatteryMeter.h"
#include "src/display/InkplateBatteryReader.h"
```

- [ ] **Step 2: Remove `ADC_OFFSET` and construct the reader**

Remove line 46:
```cpp
float ADC_OFFSET = -0.50;
```

In its place, construct the reader once (it will be used by both paths below):
```cpp
InkplateBatteryReader batteryReader(display);
```

- [ ] **Step 3: Update the error path (~lines 113–128)**

Replace:
```cpp
float rawBattery = display.readBattery();
float voltage = rawBattery + ADC_OFFSET;
```
with:
```cpp
float voltage = batteryReader.readVoltage();
```

Replace:
```cpp
display.print(voltage, 2);
display.print('V');
```
with:
```cpp
display.print(BatteryMeter::voltageToPercent(voltage));
display.print('%');
```

Delete the entire `"raw: "` debug block:
```cpp
display.setFont(Roboto_Light.at(24));
display.setCursor(860, 455);
display.print("raw: ");
display.print(rawBattery, 3);
display.print('V');
```

- [ ] **Step 4: Update the normal path (~lines 194–210)**

Replace:
```cpp
float rawBattery = display.readBattery();
voltage = rawBattery + ADC_OFFSET;
```
with:
```cpp
voltage = batteryReader.readVoltage();
```

Replace:
```cpp
display.print(voltage, 2);
display.print('V');
```
with:
```cpp
display.print(BatteryMeter::voltageToPercent(voltage));
display.print('%');
```

Delete the entire `"raw: "` debug block:
```cpp
display.setFont(Roboto_Light.at(24));
display.setCursor(860, 455);
display.print("raw: ");
display.print(rawBattery, 3);
display.print('V');
```

- [ ] **Step 5: Update the `BatteryLogger` call (~line 226)**

Replace:
```cpp
batteryLogger.log(now, rawBattery, voltage);
```
with:
```cpp
batteryLogger.log(now, voltage);
```

- [ ] **Step 6: Build firmware — must succeed**

```bash
./build.sh build
```

Expected: clean build, no errors or new warnings.

- [ ] **Step 7: Run host tests — must still pass**

```bash
./build.sh test-host
```

Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add Weather_Station_2.cpp
git commit -m "feat: replace ADC_OFFSET with IBatteryReader + BatteryMeter

- Remove ADC_OFFSET=-0.50 (empirically confirmed to be wrong)
- Wire InkplateBatteryReader + BatteryMeter in both call sites
- Display SOC% instead of raw voltage
- Remove raw voltage debug display lines
- Update BatteryLogger call to new single-arg signature

Closes #22"
```

---

## Done

Flash and verify on device:
```bash
./build.sh flash && ./build.sh monitor
```

Expected boot output includes the battery % reading in place of the old voltage display. Confirm the reading is in a plausible range (should be near 100% on a freshly charged battery).
