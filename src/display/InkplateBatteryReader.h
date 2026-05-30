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
