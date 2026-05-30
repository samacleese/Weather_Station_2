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
