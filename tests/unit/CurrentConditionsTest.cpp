// ABOUTME: Host unit tests for CurrentConditions logic.
// ABOUTME: Uses hand-written stubs for IHttpClient and IClock — no WiFi required.
#include <gtest/gtest.h>
#include "network/CurrentConditions.h"
#include "network/IClock.h"
#include "network/IHttpClient.h"

// ── Test doubles ─────────────────────────────────────────────────────────────

class StubHttpClient : public IHttpClient {
   public:
    int next_result = 0;
    std::string next_body;
    int call_count = 0;

    int get(const std::string& url, std::string& body) override {
        call_count++;
        body = next_body;
        return next_result;
    }
};

class StubClock : public IClock {
   public:
    unsigned long current_ms = 0;
    unsigned long millis() const override { return current_ms; }
};

// ── validateData() tests ──────────────────────────────────────────────────────

TEST(ValidateDataTest, ValidDataPasses) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = 10;
    ASSERT_TRUE(cc.validateData());
}

TEST(ValidateDataTest, TemperatureTooHighFails) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = 101;
    cc.dew_point = 15;
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, TemperatureTooLowFails) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = -101;
    cc.dew_point = -90;
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, DewPointTooHighFails) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 26;
    cc.wind_speed = 10;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, NegativeWindSpeedFails) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = -1;
    ASSERT_FALSE(cc.validateData());
}

TEST(ValidateDataTest, ExcessiveWindSpeedFails) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    cc.temperature = 20;
    cc.dew_point = 15;
    cc.wind_speed = 501;
    ASSERT_FALSE(cc.validateData());
}

// ── getErrorString() tests ────────────────────────────────────────────────────

TEST(GetErrorStringTest, AllCodesReturnNonNull) {
    StubHttpClient http;
    StubClock clock;
    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_OK), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_ERROR), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_JSON_ERROR), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_NETWORK_ERROR), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_DATA_MISSING), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_INVALID_DATA), nullptr);
    ASSERT_NE(cc.getErrorString(CURRENT_CONDITIONS_CERT_ERROR), nullptr);
    ASSERT_NE(cc.getErrorString(-999), nullptr);
}

// ── update() tests ────────────────────────────────────────────────────────────

static const std::string valid_json = R"({
  "properties": {
    "textDescription": "Clear",
    "rawMessage": "KBFI 010053Z 00000KT 10SM SKC 05/01 A2992",
    "temperature": { "value": 5.0, "unitCode": "wmoUnit:degC", "qualityControl": "V" },
    "dewpoint":    { "value": 1.0, "unitCode": "wmoUnit:degC", "qualityControl": "V" },
    "windSpeed":   { "value": 7.0, "unitCode": "wmoUnit:km_h-1", "qualityControl": "V" }
  }
})";

TEST(UpdateTest, SuccessfulUpdateParsesData) {
    StubHttpClient http;
    StubClock clock;
    http.next_result = 0;
    http.next_body = valid_json;

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_OK);
    ASSERT_STREQ(cc.description, "Clear");
    ASSERT_EQ(cc.temperature, 5);
    ASSERT_EQ(cc.dew_point, 1);
    ASSERT_EQ(cc.wind_speed, 7);
}

TEST(UpdateTest, NetworkErrorReturnsCCNetworkError) {
    StubHttpClient http;
    StubClock clock;
    http.next_result = -2;  // NETWORK_HTTP_ERROR value
    http.next_body = "";

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_NETWORK_ERROR);
}

TEST(UpdateTest, MalformedJsonReturnsJsonError) {
    StubHttpClient http;
    StubClock clock;
    http.next_result = 0;
    http.next_body = "not json at all {{{";

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_JSON_ERROR);
}

TEST(UpdateTest, CacheReturnedWhenClockHasNotAdvanced) {
    StubHttpClient http;
    StubClock clock;
    http.next_body = valid_json;
    http.next_result = 0;

    CurrentConditions cc(http, clock, "KBFI");
    // First update succeeds and populates cache (m_last_update_time = 0)
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_OK);

    // Simulate network failure with clock still under 30-minute threshold
    http.next_result = -2;
    clock.current_ms = 1799999;  // 1800000 - 1: within cache window

    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_NETWORK_ERROR);  // error returned
    ASSERT_STREQ(cc.description, "Clear");  // cached data still served
}

static const std::string cloudy_json = R"({
  "properties": {
    "textDescription": "Cloudy",
    "rawMessage": "KBFI 010153Z 05005KT 10SM OVC015 08/04 A2989",
    "temperature": { "value": 8.0, "unitCode": "wmoUnit:degC", "qualityControl": "V" },
    "dewpoint":    { "value": 4.0, "unitCode": "wmoUnit:degC", "qualityControl": "V" },
    "windSpeed":   { "value": 9.0, "unitCode": "wmoUnit:km_h-1", "qualityControl": "V" }
  }
})";

TEST(UpdateTest, CacheNotUsedAfter30Minutes) {
    StubHttpClient http;
    StubClock clock;
    http.next_body = valid_json;
    http.next_result = 0;

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_OK);
    ASSERT_STREQ(cc.description, "Clear");

    // Advance past cache window; serve different JSON to prove a real fetch occurs
    clock.current_ms = 1800001;
    http.next_body = cloudy_json;
    http.next_result = 0;

    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_OK);
    // If cache had been returned, description would still be "Clear".
    ASSERT_STREQ(cc.description, "Cloudy");
    ASSERT_EQ(http.call_count, 2);  // both updates hit the network
}
