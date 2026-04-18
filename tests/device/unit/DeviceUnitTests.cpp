// ABOUTME: Device unit test sketch using AUnit with the GoogleTest macro adapter.
// ABOUTME: Flash to Inkplate and monitor serial at 115200 baud for PASS/FAIL results.
#include <AUnit.h>
#include <aunit/contrib/gtest.h>
#include <Arduino.h>
#include <memory>

#include "display/Kitties.h"
#include "network/CurrentConditions.h"

// Constraint: use only ASSERT_* macros (not EXPECT_*); TEST_F() is not
// supported by the AUnit gtest adapter.

// ── Kitties tests ────────────────────────────────────────────────────────────
// m_count is RTC_DATA_ATTR — resets to 0 on power cycle, persists across deep
// sleep. This sketch uses no deep sleep, so m_count starts at 0 each run.
// Tests use relative assertions to be independent of starting counter state.

TEST(KittiesTest, CyclesThroughFiveImages) {
    const uint8_t* a = Kitties::getNextKitty();
    for (int i = 0; i < 4; i++) Kitties::getNextKitty();
    // 5 calls = one full cycle; next call should return the same image as a.
    const uint8_t* b = Kitties::getNextKitty();
    ASSERT_EQ(a, b);
}

TEST(KittiesTest, Uint8OverflowDoesNotBreakCycling) {
    // 256 calls wraps uint8_t m_count back to its pre-call value (N+256 mod 256 = N).
    for (int i = 0; i < 256; i++) Kitties::getNextKitty();
    // Verify the 5-cycle invariant still holds after overflow.
    const uint8_t* a = Kitties::getNextKitty();
    for (int i = 0; i < 4; i++) Kitties::getNextKitty();
    const uint8_t* b = Kitties::getNextKitty();
    ASSERT_EQ(a, b);
}

// ── CurrentConditions::validateData() tests ──────────────────────────────────
// Constructs CurrentConditions with nullptr network; constructor stores the
// pointer without dereferencing it (dereference only occurs in update()).

TEST(ValidateDataTest, ValidDataPasses) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = 10;
    ASSERT_TRUE(cc.validateData());
}

TEST(ValidateDataTest, TemperatureTooHighFails) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = 101;
    cc.dew_point = 15;
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, TemperatureTooLowFails) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = -101;
    cc.dew_point = -90;
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, DewPointTooHighFails) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 26;  // more than temperature + 5
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, NegativeWindSpeedFails) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = -1;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, ExcessiveWindSpeedFails) {
    CurrentConditions cc(nullptr, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = 501;
    ASSERT_FALSE(cc.validateData());
}

// ── CurrentConditions::getErrorString() tests ────────────────────────────────

TEST(GetErrorStringTest, AllCodesReturnNonNull) {
    CurrentConditions cc(nullptr, "KBFI");
    // ASSERT_NE(..., nullptr) doesn't work in AUnit gtest adapter (nullptr_t cast)
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_OK) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_ERROR) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_JSON_ERROR) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_NETWORK_ERROR) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_DATA_MISSING) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_INVALID_DATA) != nullptr);
    ASSERT_TRUE(cc.getErrorString(CURRENT_CONDITIONS_CERT_ERROR) != nullptr);
    ASSERT_TRUE(cc.getErrorString(-999) != nullptr);  // unknown code → "Unknown error"
}

TEST(GetErrorStringTest, OkCodeReturnsCorrectString) {
    CurrentConditions cc(nullptr, "KBFI");
    // Cast literal to const char* so decltype(a) resolves correctly in AUnit adapter
    ASSERT_STREQ(cc.getErrorString(CURRENT_CONDITIONS_OK), (const char*)"No error");
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
}

void loop() {
    aunit::TestRunner::run();
}
