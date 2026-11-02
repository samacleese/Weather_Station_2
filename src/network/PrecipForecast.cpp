// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#include "PrecipForecast.h"

#include <cctype>
#include <cstdio>

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil).
static long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}

bool PrecipForecast::weekdayAbbrev(const char* isoDate, char* out, size_t len) {
    static const char* const names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (isoDate == nullptr || out == nullptr) return false;

    int year, month, day;
    if (sscanf(isoDate, "%4d-%2d-%2d", &year, &month, &day) != 3) return false;
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;

    // 1970-01-01 was a Thursday (index 4). The double modulo keeps negative day counts in range.
    long days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    int weekday = static_cast<int>(((days + 4) % 7 + 7) % 7);
    snprintf(out, len, "%s", names[weekday]);
    return true;
}

bool PrecipForecast::mentionsSnow(const char* text) {
    static const char needle[] = "snow";
    if (text == nullptr) return false;
    for (const char* p = text; *p; ++p) {
        size_t i = 0;
        while (needle[i] && p[i] && std::tolower(static_cast<unsigned char>(p[i])) == needle[i]) ++i;
        if (needle[i] == '\0') return true;
    }
    return false;
}

void PrecipForecast::formatDay(const PrecipDay& day, char* buf, size_t len) {
    if (day.percent < 0) {
        snprintf(buf, len, "%s --%%", day.label);
    } else {
        snprintf(buf, len, "%s %d%%", day.label, day.percent);
    }
}
