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
