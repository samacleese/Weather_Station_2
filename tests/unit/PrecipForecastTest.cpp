// ABOUTME: Host unit tests for PrecipForecast date, snow, and formatting helpers.
// ABOUTME: Also covers update() fetch and parse via StubHttpClient with real captured weather.gov responses.
#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#include "StubHttpClient.h"
#include "network/PrecipForecast.h"

// ── weekdayAbbrev() ─────────────────────────────────────────────────────────

static std::string weekdayOf(const char* isoDate) {
    char out[6] = "";
    if (!PrecipForecast::weekdayAbbrev(isoDate, out, sizeof(out))) return "<fail>";
    return out;
}

TEST(WeekdayAbbrevTest, KnownDates) {
    ASSERT_EQ(weekdayOf("1970-01-01"), "Thu");
    ASSERT_EQ(weekdayOf("2000-02-29"), "Tue");
    ASSERT_EQ(weekdayOf("2026-11-02T10:00:00-08:00"), "Mon");
    ASSERT_EQ(weekdayOf("2026-11-03T06:00:00-08:00"), "Tue");
    ASSERT_EQ(weekdayOf("2026-11-26T06:00:00-08:00"), "Thu");  // Thanksgiving 2026
    ASSERT_EQ(weekdayOf("2027-01-03"), "Sun");
    ASSERT_EQ(weekdayOf("2027-01-02"), "Sat");
}

TEST(WeekdayAbbrevTest, MalformedDatesFail) {
    ASSERT_EQ(weekdayOf(""), "<fail>");
    ASSERT_EQ(weekdayOf("garbage"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-13-01"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-11-32"), "<fail>");
    ASSERT_FALSE(PrecipForecast::weekdayAbbrev(nullptr, nullptr, 0));
}

TEST(WeekdayAbbrevTest, DaysPastEndOfMonthFail) {
    ASSERT_EQ(weekdayOf("2026-02-30"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-02-29"), "<fail>");  // 2026 is not a leap year
    ASSERT_EQ(weekdayOf("1900-02-29"), "<fail>");  // century, not divisible by 400
    ASSERT_EQ(weekdayOf("2026-04-31"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-11-31"), "<fail>");
}

TEST(WeekdayAbbrevTest, LastDayOfMonthPasses) {
    ASSERT_EQ(weekdayOf("2028-02-29"), "Tue");  // leap year
    ASSERT_EQ(weekdayOf("2026-04-30"), "Thu");
    ASSERT_EQ(weekdayOf("2026-12-31"), "Thu");
}

// ── mentionsSnow() ──────────────────────────────────────────────────────────

TEST(MentionsSnowTest, DetectsSnowAnyCase) {
    ASSERT_TRUE(PrecipForecast::mentionsSnow("Light Snow Likely"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("Mostly Sunny then Rain And Snow Likely"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("SNOW"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("snow showers"));
}

TEST(MentionsSnowTest, IgnoresNonSnow) {
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Chance Light Rain"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Partly Sunny"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Sno"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow(""));
    ASSERT_FALSE(PrecipForecast::mentionsSnow(nullptr));
}

// ── formatDay() ─────────────────────────────────────────────────────────────

TEST(FormatDayTest, KnownPercent) {
    PrecipDay day = {"Wed", 87, false};
    char buf[16];
    PrecipForecast::formatDay(day, buf, sizeof(buf));
    ASSERT_STREQ(buf, "Wed 87%");
}

TEST(FormatDayTest, UnknownPercent) {
    PrecipDay day = {"Today", -1, true};
    char buf[16];
    PrecipForecast::formatDay(day, buf, sizeof(buf));
    ASSERT_STREQ(buf, "Today --%");
}

// ── update() ────────────────────────────────────────────────────────────────

static std::string readFixture(const char* name) {
    std::ifstream in(std::string(FIXTURE_DIR) + "/" + name);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Builds a minimal forecast body. Each entry: startTime, isDaytime, percent (-1 = null), shortForecast.
struct PeriodSpec {
    const char* start;
    bool daytime;
    int percent;
    const char* shortForecast;
};

static std::string forecastJson(std::initializer_list<PeriodSpec> periods) {
    std::string json = R"({"properties":{"periods":[)";
    bool first = true;
    for (const PeriodSpec& p : periods) {
        if (!first) json += ",";
        first = false;
        json += std::string(R"({"name":"ignored","startTime":")") + p.start + R"(","isDaytime":)" +
                (p.daytime ? "true" : "false") + R"(,"probabilityOfPrecipitation":{"unitCode":"wmoUnit:percent","value":)" +
                (p.percent < 0 ? std::string("null") : std::to_string(p.percent)) + R"(},"shortForecast":")" +
                p.shortForecast + R"("})";
    }
    json += "]}}";
    return json;
}

TEST(PrecipUpdateTest, BuildsGridpointUrl) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    PrecipForecast forecast(http, "SEW/125,65");
    forecast.update();
    ASSERT_EQ(http.last_url, "https://api.weather.gov/gridpoints/SEW/125,65/forecast");
}

TEST(PrecipUpdateTest, ParsesRealKbfiMorningForecast) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    ASSERT_FALSE(http.next_body.empty()) << "fixture missing";
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    const char* labels[] = {"Today", "Tue", "Wed", "Thu"};
    int percents[] = {1, 0, 87, 25};
    for (int i = 0; i < 4; i++) {
        EXPECT_STREQ(forecast.days[i].label, labels[i]) << "day " << i;
        EXPECT_EQ(forecast.days[i].percent, percents[i]) << "day " << i;
        EXPECT_FALSE(forecast.days[i].snow) << "day " << i;
    }
}

TEST(PrecipUpdateTest, FlagsSnowInRealCascadesForecast) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_cascades_2026-11-02.json");
    ASSERT_FALSE(http.next_body.empty()) << "fixture missing";
    PrecipForecast forecast(http, "SEW/166,121");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    bool snow[] = {false, false, true, true};
    int percents[] = {0, 0, 57, 55};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(forecast.days[i].snow, snow[i]) << "day " << i;
        EXPECT_EQ(forecast.days[i].percent, percents[i]) << "day " << i;
    }
}

TEST(PrecipUpdateTest, EveningForecastStartsTomorrowAndSkipsNights) {
    StubHttpClient http;
    http.next_body = forecastJson({
        {"2026-11-02T18:00:00-08:00", false, 10, "Rain"},
        {"2026-11-03T06:00:00-08:00", true, 20, "Rain"},
        {"2026-11-03T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-04T06:00:00-08:00", true, 30, "Rain"},
        {"2026-11-04T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-05T06:00:00-08:00", true, 40, "Rain"},
        {"2026-11-05T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-06T06:00:00-08:00", true, -1, "Cloudy"},
        {"2026-11-06T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-07T06:00:00-08:00", true, 50, "Rain"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    EXPECT_STREQ(forecast.days[0].label, "Tue");
    EXPECT_EQ(forecast.days[0].percent, 20);
    EXPECT_STREQ(forecast.days[1].label, "Wed");
    EXPECT_STREQ(forecast.days[2].label, "Thu");
    EXPECT_STREQ(forecast.days[3].label, "Fri");
    EXPECT_EQ(forecast.days[3].percent, -1);  // null PoP
}

TEST(PrecipUpdateTest, HolidayPeriodGetsWeekdayLabel) {
    StubHttpClient http;
    // The period name would be "Thanksgiving Day". The label must come from the date.
    http.next_body = forecastJson({
        {"2026-11-25T18:00:00-08:00", false, 10, "Cloudy"},
        {"2026-11-26T06:00:00-08:00", true, 40, "Chance Rain"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 1);
    EXPECT_STREQ(forecast.days[0].label, "Thu");
}

TEST(PrecipUpdateTest, MalformedStartTimeGetsPlaceholderLabel) {
    StubHttpClient http;
    http.next_body = forecastJson({
        {"2026-11-02T10:00:00-08:00", true, 5, "Sunny"},
        {"not-a-date", true, 15, "Sunny"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 2);
    EXPECT_STREQ(forecast.days[0].label, "Today");
    EXPECT_STREQ(forecast.days[1].label, "---");
}

TEST(PrecipUpdateTest, NoDaytimePeriodsIsDataMissing) {
    StubHttpClient http;
    http.next_body = forecastJson({{"2026-11-02T18:00:00-08:00", false, 10, "Rain"}});
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_DATA_MISSING);
    ASSERT_EQ(forecast.dayCount, 0);
}

TEST(PrecipUpdateTest, MissingPropertiesIsDataMissing) {
    StubHttpClient http;
    http.next_body = R"({"type":"Feature"})";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_DATA_MISSING);
}

TEST(PrecipUpdateTest, MalformedJsonIsJsonError) {
    StubHttpClient http;
    http.next_body = "not json at all {{{";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_JSON_ERROR);
}

TEST(PrecipUpdateTest, EmptyBodyIsNetworkError) {
    StubHttpClient http;
    http.next_body = "";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
}

TEST(PrecipUpdateTest, NetworkResultsMapToErrorCodes) {
    StubHttpClient http;
    PrecipForecast forecast(http, "SEW/125,65");
    http.next_result = -2;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
    http.next_result = -5;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_CERT_ERROR);
    http.next_result = -99;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_ERROR);
    EXPECT_EQ(forecast.lastError, PRECIP_FORECAST_ERROR);
}

TEST(PrecipUpdateTest, FailureAfterSuccessClearsDays) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);

    http.next_result = -2;
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
    ASSERT_EQ(forecast.dayCount, 0);
}

TEST(PrecipUpdateTest, AllErrorStringsNonNull) {
    StubHttpClient http;
    PrecipForecast forecast(http, "SEW/125,65");
    for (int code : {PRECIP_FORECAST_OK, PRECIP_FORECAST_ERROR, PRECIP_FORECAST_JSON_ERROR,
                     PRECIP_FORECAST_NETWORK_ERROR, PRECIP_FORECAST_DATA_MISSING, PRECIP_FORECAST_CERT_ERROR, -999}) {
        ASSERT_NE(forecast.getErrorString(code), nullptr) << "code " << code;
    }
}
