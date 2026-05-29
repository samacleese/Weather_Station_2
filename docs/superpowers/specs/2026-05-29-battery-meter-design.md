# Battery Meter Design

**Date:** 2026-05-29
**Issue:** #22 — Battery voltage reads low; ADC_OFFSET needs recalibration

## Background

The device has been logging `{ts, raw, adj}` battery readings to a local JSON store since
2026-04-12 via `BatteryLogger`. A full discharge-and-recharge cycle has now been captured
(43.4 days, 6,073 readings). Analysis of this data reveals:

- `ADC_OFFSET = -0.50` is wrong. The raw `readBattery()` output is already accurate:
  the first raw reading (4.294V on April 12) matches the known full-charge voltage
  (4.296V from April 11) to within 2mV.
- The correct offset is **0** — it should be removed entirely.
- A full empirical voltage → SOC% lookup table can be derived from the discharge curve.

## Goals

1. Remove `ADC_OFFSET` and display correct battery voltage.
2. Replace the voltage display with a 0–100% battery meter using the empirical lookup table.
3. Separate the ADC hardware call behind an interface for testability.
4. Keep `BatteryLogger` running to verify % readings over the next discharge cycle.

## Empirical Lookup Table

Derived from 43.4-day discharge curve. 0% is anchored at 3.224V — the last stable voltage
before the board stopped successfully refreshing the display (the board went dark between
3.224V and 3.104V).

| Voltage (V) | SOC % |
|-------------|-------|
| 4.294       | 100   |
| 4.188       |  90   |
| 4.118       |  80   |
| 4.096       |  70   |
| 3.994       |  60   |
| 3.954       |  50   |
| 3.890       |  40   |
| 3.836       |  30   |
| 3.704       |  20   |
| 3.578       |  10   |
| 3.224       |   0   |

The curve is mostly linear in the 20–80% range, with the classic LiPo plateau at 70–80%
(only 22mV across 10% of capacity) and steep drops at both ends.

## Architecture

Three new/changed units:

```
src/display/BatteryReader.h              ← new: abstract interface (header-only)
src/display/InkplateBatteryReader.h/.cpp ← new: hardware adapter
src/display/BatteryMeter.h/.cpp          ← new: lookup table + voltageToPercent()
Weather_Station_2.cpp                    ← changed: remove ADC_OFFSET, use reader+meter
tests/unit/BatteryMeterTest.cpp          ← new: host unit tests
tests/unit/CMakeLists.txt                ← changed: add BatteryMeterTest + BatteryMeter.cpp
```

## Component Design

### `BatteryReader` (header-only interface)

```cpp
class BatteryReader {
public:
    virtual ~BatteryReader() = default;
    virtual float readVoltage() const = 0;
};
```

Pure abstract. No state. Lives in `src/display/BatteryReader.h`.

### `InkplateBatteryReader` (hardware adapter)

```cpp
// InkplateBatteryReader.h must #include "BatteryReader.h"
class InkplateBatteryReader : public BatteryReader {
public:
    explicit InkplateBatteryReader(Inkplate& display);
    float readVoltage() const override;  // calls display.readBattery()
};
```

The only place `display.readBattery()` is called after migration. Takes `Inkplate&` by
reference — does not own the display.

Placement note: `BatteryReader`, `InkplateBatteryReader`, and `BatteryMeter` live in
`src/display/` rather than `src/network/` because they serve display rendering (battery %
is a display element). `BatteryLogger` in `src/network/` is a network concern (POSTing
data); the two responsibilities don't overlap.

### `BatteryMeter` (static utility)

```cpp
class BatteryMeter {
public:
    static int voltageToPercent(float voltage);
};
```

Owns the lookup table as a file-scope `static const` array of `{voltage, percent}` pairs
sorted descending by voltage. `voltageToPercent` walks the table, finds the bracketing pair,
linearly interpolates, and rounds to the nearest integer before returning. Clamped: below
3.224V returns 0, above 4.294V returns 100.

`BatteryMeter` and `BatteryReader` are intentionally decoupled — the meter is pure math
with no hardware dependency.

### `Weather_Station_2.cpp` changes

There are two `readBattery()` call sites:

**Error path** (inside `if (updateResult != CURRENT_CONDITIONS_OK)` > inner `else`, around
line 113): this branch runs only when there is no weather data at all. It reads the battery,
displays voltage + temperature on one line, then deep-sleeps. `BatteryLogger` is NOT called
here.

**Normal path** (around line 194): runs after weather data is fetched. Reads the battery,
displays voltage + temperature, then calls `BatteryLogger`.

Changes to make at both call sites:

- Add includes near the top of `Weather_Station_2.cpp`:
  ```cpp
  #include "src/display/BatteryMeter.h"
  #include "src/display/InkplateBatteryReader.h"
  ```
  (`BatteryReader.h` is included transitively because `InkplateBatteryReader.h` must `#include "BatteryReader.h"` — see Component Design above.)
- Remove the `ADC_OFFSET` constant declaration (line 46) and all uses.
- Replace `display.readBattery()` with `reader.readVoltage()` where `reader` is an
  `InkplateBatteryReader` constructed once before both paths: 
  `InkplateBatteryReader reader(display);`
- `rawBattery` is no longer needed. Remove it from both scopes.
  - **Error path**: replace `float rawBattery = display.readBattery(); float voltage = rawBattery + ADC_OFFSET;` with `float voltage = reader.readVoltage();` (new local declaration, same as before).
  - **Normal path**: `voltage` is already declared at outer scope (line 191). Replace `float rawBattery = display.readBattery(); voltage = rawBattery + ADC_OFFSET;` with the assignment `voltage = reader.readVoltage();` — no new declaration.
- Each path prints battery + temperature together on one line:
  `display.print(voltage, 2); display.print('V'); display.print(' '); display.print(temperature, DEC); display.print('C');`
  Replace the voltage portion only:
  `display.print(BatteryMeter::voltageToPercent(voltage)); display.print('%'); display.print(' '); display.print(temperature, DEC); display.print('C');`
- Remove the `"raw: "` debug block (lines 124–128 in error path, lines 206–210 in normal
  path) entirely.
- In the normal path, update the `BatteryLogger` call from
  `batteryLogger.log(now, rawBattery, voltage)` to `batteryLogger.log(now, voltage, voltage)`.
  Both arguments are the same because the offset is zero. The `adj` field is vestigial;
  keeping the log schema unchanged preserves the existing 6,000-entry historical dataset.

## Error Handling

`voltageToPercent` is pure math with no I/O. Out-of-range voltages (including 0V or
pathological values from a hardware fault) are clamped to 0–100% without crashing.

## Testing

`tests/unit/BatteryMeterTest.cpp` — host unit tests, no Inkplate dependency.

`tests/unit/CMakeLists.txt` must be updated to add `BatteryMeterTest.cpp` and
`../../src/display/BatteryMeter.cpp` to the `add_executable` sources. Do **not** add
`InkplateBatteryReader.cpp` — it depends on `Inkplate.h` which is not available in the
host build.

Test cases for `BatteryMeter::voltageToPercent`:

- Each of the 11 breakpoint voltages returns its exact percentage.
- Midpoint between two adjacent breakpoints interpolates correctly.
- Voltage below 3.224V clamps to 0%.
- Voltage above 4.294V clamps to 100%.

`InkplateBatteryReader` is not unit-tested directly — it is a one-line wrapper around a
hardware call, verified by running the device.

## Out of Scope

- Removing `BatteryLogger` — kept for next-cycle verification.
- Hysteresis or smoothing on the % reading.
- Displaying a graphical battery icon (future issue).
