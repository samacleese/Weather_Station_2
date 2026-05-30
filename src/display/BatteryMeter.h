// ABOUTME: Converts battery voltage to state-of-charge percentage.
// ABOUTME: Uses an empirical lookup table derived from a full discharge cycle.
#ifndef BATTERY_METER_H
#define BATTERY_METER_H

class BatteryMeter {
public:
    static int voltageToPercent(float voltage);
};

#endif  // BATTERY_METER_H
