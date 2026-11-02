// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#ifndef PRECIP_FORECAST_H
#define PRECIP_FORECAST_H

#include <stddef.h>

struct PrecipDay {
    char label[6];  // "Today" or a weekday abbreviation like "Tue"
    int percent;    // chance of precipitation; -1 when the API reports null
    bool snow;      // shortForecast mentions snow
};

class PrecipForecast {
   public:
    // Writes the 3-letter weekday of the YYYY-MM-DD prefix of isoDate. Returns false if the date is malformed.
    static bool weekdayAbbrev(const char* isoDate, char* out, size_t len);
    // True when text contains "snow", ignoring case.
    static bool mentionsSnow(const char* text);
    // Formats one day for the display strip, e.g. "Wed 87%" or "Fri --%".
    static void formatDay(const PrecipDay& day, char* buf, size_t len);
};

#endif  // PRECIP_FORECAST_H
