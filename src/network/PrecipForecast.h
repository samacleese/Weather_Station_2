// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#ifndef PRECIP_FORECAST_H
#define PRECIP_FORECAST_H

#include <ArduinoJson.h>
#include <stddef.h>

#include <string>

#include "IHttpClient.h"

const int PRECIP_FORECAST_OK = 0;
const int PRECIP_FORECAST_ERROR = -1;
const int PRECIP_FORECAST_JSON_ERROR = -2;
const int PRECIP_FORECAST_NETWORK_ERROR = -3;
const int PRECIP_FORECAST_DATA_MISSING = -4;
const int PRECIP_FORECAST_CERT_ERROR = -5;

struct PrecipDay {
    char label[6];  // "Today" or a weekday abbreviation like "Tue"
    int percent;    // chance of precipitation; -1 when the API reports null
    bool snow;      // shortForecast mentions snow
};

class PrecipForecast {
   public:
    static const int MAX_DAYS = 4;

    // gridpoint is "{wfo}/{x},{y}", e.g. "SEW/125,65" (from GET /points/{lat},{lon}).
    PrecipForecast(IHttpClient& http, const std::string& gridpoint);

    // Fetches the forecast and fills days[] with up to MAX_DAYS daytime periods. Returns a PRECIP_FORECAST_* code.
    int update();
    const char* getErrorString(int errorCode);

    // Writes the 3-letter weekday of the YYYY-MM-DD prefix of isoDate. Returns false if the date is malformed.
    static bool weekdayAbbrev(const char* isoDate, char* out, size_t len);
    // True when text contains "snow", ignoring case.
    static bool mentionsSnow(const char* text);
    // Formats one day for the display strip, e.g. "Wed 87%" or "Fri --%".
    static void formatDay(const PrecipDay& day, char* buf, size_t len);

    PrecipDay days[MAX_DAYS];
    int dayCount = 0;
    int lastError = PRECIP_FORECAST_OK;

   private:
    int parse(const std::string& input);

    IHttpClient& m_http;
    std::string m_url;
    DynamicJsonDocument m_doc;
    StaticJsonDocument<256> m_filter;
};

#endif  // PRECIP_FORECAST_H
