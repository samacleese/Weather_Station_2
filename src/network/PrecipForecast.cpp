// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#include "PrecipForecast.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "NetworkLog.h"
#include "NetworkResult.h"

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

static const char* url_prefix = "https://api.weather.gov/gridpoints/";
// The filtered forecast (14 periods x 4 fields) needs about 3 KB on the 64-bit host; the ESP32 needs less.
static const size_t json_capacity = 6144;
static const size_t iso_date_length = 10;  // "YYYY-MM-DD"

PrecipForecast::PrecipForecast(IHttpClient& http, const std::string& gridpoint)
    : m_http(http), m_url(std::string(url_prefix) + gridpoint + "/forecast"), m_doc(json_capacity) {
    // ArduinoJson applies the filter for element 0 to every element of the array.
    JsonObject period = m_filter["properties"]["periods"].createNestedObject();
    period["startTime"] = true;
    period["isDaytime"] = true;
    period["probabilityOfPrecipitation"]["value"] = true;
    period["shortForecast"] = true;

    NET_LOG_NOTICE("Set Precip Forecast URL to %s", m_url.c_str());
}

int PrecipForecast::update() {
    dayCount = 0;
    std::string body;

    NET_LOG_NOTICE("Getting Precip Forecast");
    int networkResult = m_http.get(m_url, body);

    if (networkResult != 0) {
        NET_LOG_ERROR("Network error code: %d", networkResult);
        switch (classifyNetworkResult(networkResult)) {
            case NetworkFailure::Connection:
                lastError = PRECIP_FORECAST_NETWORK_ERROR;
                break;
            case NetworkFailure::Certificate:
                lastError = PRECIP_FORECAST_CERT_ERROR;
                break;
            default:
                lastError = PRECIP_FORECAST_ERROR;
        }
        return lastError;
    }

    if (body.empty()) {
        NET_LOG_ERROR("Empty forecast response from server");
        lastError = PRECIP_FORECAST_NETWORK_ERROR;
        return lastError;
    }

    lastError = parse(body);
    return lastError;
}

int PrecipForecast::parse(const std::string& input) {
    m_doc.clear();
    DeserializationError error =
        deserializeJson(m_doc, input.c_str(), input.size(), DeserializationOption::Filter(m_filter));
    if (error) {
        NET_LOG_ERROR("Forecast deserializeJson() failed: %s", error.c_str());
        return PRECIP_FORECAST_JSON_ERROR;
    }

    JsonArray periods = m_doc["properties"]["periods"];
    if (periods.isNull()) {
        NET_LOG_ERROR("Missing properties.periods in forecast response");
        return PRECIP_FORECAST_DATA_MISSING;
    }

    // "Today" is any daytime period that starts on the same date as the first period.
    const char* firstStart = periods[0]["startTime"] | "";
    bool firstHasDate = strlen(firstStart) >= iso_date_length;

    for (JsonObject period : periods) {
        if (dayCount >= MAX_DAYS) break;
        if (!(period["isDaytime"] | false)) continue;

        PrecipDay& day = days[dayCount];
        const char* start = period["startTime"] | "";
        if (firstHasDate && strncmp(start, firstStart, iso_date_length) == 0) {
            snprintf(day.label, sizeof(day.label), "Today");
        } else if (!weekdayAbbrev(start, day.label, sizeof(day.label))) {
            snprintf(day.label, sizeof(day.label), "---");
        }

        JsonVariant pop = period["probabilityOfPrecipitation"]["value"];
        day.percent = pop.isNull() ? -1 : pop.as<int>();
        day.snow = mentionsSnow(period["shortForecast"] | "");
        NET_LOG_INFO("Precip %s: %d%%%s", day.label, day.percent, day.snow ? " (snow)" : "");
        dayCount++;
    }

    if (dayCount == 0) {
        NET_LOG_ERROR("No daytime periods in forecast response");
        return PRECIP_FORECAST_DATA_MISSING;
    }
    return PRECIP_FORECAST_OK;
}

// clang-format off
const char* PrecipForecast::getErrorString(int errorCode) {
    switch (errorCode) {
        case PRECIP_FORECAST_OK:            return "No error";
        case PRECIP_FORECAST_ERROR:         return "General error";
        case PRECIP_FORECAST_JSON_ERROR:    return "JSON parsing error";
        case PRECIP_FORECAST_NETWORK_ERROR: return "Network connection error";
        case PRECIP_FORECAST_DATA_MISSING:  return "Required data missing";
        case PRECIP_FORECAST_CERT_ERROR:    return "SSL certificate validation failed - check CA cert or run cert update";
        default:                            return "Unknown error";
    }
}
// clang-format on
