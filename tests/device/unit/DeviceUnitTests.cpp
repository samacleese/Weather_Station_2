// ABOUTME: Device unit test sketch using AUnit with the GoogleTest macro adapter.
// ABOUTME: Flash to Inkplate and monitor serial at 115200 baud for PASS/FAIL results.
#include <AUnit.h>
#include <aunit/contrib/gtest.h>
#include <Arduino.h>

// Constraint: use only ASSERT_* macros (not EXPECT_*); TEST_F() is not
// supported by the AUnit gtest adapter.

void setup() {
    Serial.begin(115200);
    while (!Serial);
}

void loop() {
    aunit::TestRunner::run();
}
