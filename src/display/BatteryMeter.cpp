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
