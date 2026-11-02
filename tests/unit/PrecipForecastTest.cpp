// ABOUTME: Host unit tests for PrecipForecast date, snow, and formatting helpers.
// ABOUTME: Pure functions only; fetch and parse coverage arrives with the HTTP client.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

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
