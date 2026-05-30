// ABOUTME: Reads battery voltage from Inkplate hardware via display.readBattery().
// ABOUTME: Implements IBatteryReader for use in production firmware.
#include "InkplateBatteryReader.h"

InkplateBatteryReader::InkplateBatteryReader(Inkplate& display) : m_display(display) {}

float InkplateBatteryReader::readVoltage() const {
    return m_display.readBattery();
}
