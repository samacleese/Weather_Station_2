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
src/display/BatteryReader.h             ← new: abstract interface (header-only)
src/display/InkplateBatteryReader.h/.cpp ← new: hardware adapter
src/display/BatteryMeter.h/.cpp         ← new: lookup table + voltageToPercent()
Weather_Station_2.cpp                   ← changed: remove ADC_OFFSET, use reader+meter
test/BatteryMeterTest.cpp               ← new: host unit tests
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
class InkplateBatteryReader : public BatteryReader {
public:
    explicit InkplateBatteryReader(Inkplate& display);
    float readVoltage() const override;  // calls display.readBattery()
};
```

The only place `display.readBattery()` is called. Takes `Inkplate&` by reference — does not
own the display.

### `BatteryMeter` (static utility)

```cpp
class BatteryMeter {
public:
    static int voltageToPercent(float voltage);
};
```

Owns the lookup table as a file-scope `static const` array of `{voltage, percent}` pairs
sorted descending by voltage. `voltageToPercent` walks the table, finds the bracketing pair,
and linearly interpolates. Clamped: below 3.224V returns 0, above 4.294V returns 100.

`BatteryMeter` and `BatteryReader` are intentionally decoupled — the meter is pure math
with no hardware dependency.

### `Weather_Station_2.cpp` changes

- Remove `ADC_OFFSET` constant and all uses.
- Construct `InkplateBatteryReader reader(display)`.
- Call `float voltage = reader.readVoltage()` where `readBattery()` was called.
- Replace `display.print("raw: ...")` lines with `display.print(BatteryMeter::voltageToPercent(voltage)); display.print("%")`.
- Pass `voltage` to `BatteryLogger::log(now, voltage, voltage)` — both raw and adj are the
  same now that offset is zero; the adj field is vestigial but we keep the log format
  unchanged to avoid breaking the existing data schema.

## Error Handling

`voltageToPercent` is pure math with no I/O. Out-of-range voltages (including 0V or
pathological values from a hardware fault) are clamped to 0–100% without crashing.

## Testing

`test/BatteryMeterTest.cpp` — host unit tests, no Inkplate dependency.

A small inline stub satisfies `BatteryReader` for any tests that need it:

```cpp
struct StubBatteryReader : public BatteryReader {
    float voltage;
    float readVoltage() const override { return voltage; }
};
```

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
