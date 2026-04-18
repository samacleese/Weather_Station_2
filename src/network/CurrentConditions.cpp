// ABOUTME: Weather.gov API client for fetching current conditions from a NWS station.
// ABOUTME: Parses observation JSON and exposes temperature, wind, dew point, and description.
#include "CurrentConditions.h"

#include <cstdio>

#ifdef ARDUINO
#include <ArduinoLog.h>
#define CC_LOG_ERROR(fmt, ...) Log.errorln(fmt, ##__VA_ARGS__)
#define CC_LOG_WARNING(fmt, ...) Log.warningln(fmt, ##__VA_ARGS__)
#define CC_LOG_NOTICE(fmt, ...) Log.noticeln(fmt, ##__VA_ARGS__)
#define CC_LOG_INFO(fmt, ...) Log.infoln(fmt, ##__VA_ARGS__)
#else
#define CC_LOG_ERROR(fmt, ...)
#define CC_LOG_WARNING(fmt, ...)
#define CC_LOG_NOTICE(fmt, ...)
#define CC_LOG_INFO(fmt, ...)
#endif

static const char* url_format = "https://api.weather.gov/stations/%s/observations/latest";
static unsigned int json_size = 768;

CurrentConditions::CurrentConditions(IHttpClient& http, IClock& clock, const std::string& station)
    : m_http(http), m_clock(clock), m_doc(json_size) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), url_format, station.c_str());
    m_url = buffer;

    JsonObject filter_properties = m_filter.createNestedObject("properties");
    filter_properties["textDescription"] = true;
    filter_properties["rawMessage"] = true;
    filter_properties["windSpeed"] = true;
    filter_properties["dewpoint"] = true;
    filter_properties["temperature"] = true;

    CC_LOG_NOTICE("Set Current Conditions URL to %s", m_url.c_str());
}

int CurrentConditions::update(int retries) {
    lastError = CURRENT_CONDITIONS_OK;
    std::string body;

    CC_LOG_NOTICE("Getting Current Conditions");

    int networkResult = m_http.get(m_url, body);

    if (networkResult != 0) {
        CC_LOG_ERROR("Network error code: %d", networkResult);

        // Map NetworkError enum values (defined in Network.h) by their integer
        // values. CurrentConditions no longer includes Network.h to keep host
        // compilation clean. These values must stay in sync with NetworkError.
        switch (networkResult) {
            case -1: case -2: case -3: case -4:
                lastError = CURRENT_CONDITIONS_NETWORK_ERROR;
                break;
            case -5:
                lastError = CURRENT_CONDITIONS_CERT_ERROR;
                break;
            default:
                lastError = CURRENT_CONDITIONS_ERROR;
        }

        if (m_last_valid.valid && (m_clock.millis() - m_last_update_time < 1800000)) {
            CC_LOG_WARNING("Using cached weather data from previous update");
            description = m_last_valid.description.c_str();
            raw_message = m_last_valid.raw_message.c_str();
            temperature = m_last_valid.temperature;
            dew_point = m_last_valid.dew_point;
            wind_speed = m_last_valid.wind_speed;
            return lastError;
        }

        return lastError;
    }

    if (body.empty()) {
        CC_LOG_ERROR("Empty response from server");
        lastError = CURRENT_CONDITIONS_NETWORK_ERROR;
        return lastError;
    }

    int parseResult = this->parse(body);

    if (parseResult == CURRENT_CONDITIONS_OK && !validateData()) {
        parseResult = CURRENT_CONDITIONS_INVALID_DATA;
    }

    if (parseResult == CURRENT_CONDITIONS_OK) {
        m_last_valid.description = description;
        m_last_valid.raw_message = raw_message;
        m_last_valid.temperature = temperature;
        m_last_valid.dew_point = dew_point;
        m_last_valid.wind_speed = wind_speed;
        m_last_valid.valid = true;
        m_last_update_time = m_clock.millis();
    }

    lastError = parseResult;
    return parseResult;
}

bool CurrentConditions::validateData() {
    if (temperature < -100 || temperature > 100) {
        CC_LOG_ERROR("Invalid temperature value: %d", temperature);
        return false;
    }
    if (dew_point < -100 || dew_point > 100 || dew_point > temperature + 5) {
        CC_LOG_ERROR("Invalid dew point value: %d", dew_point);
        return false;
    }
    if (wind_speed < 0 || wind_speed > 500) {
        CC_LOG_ERROR("Invalid wind speed value: %d", wind_speed);
        return false;
    }
    return true;
}

int CurrentConditions::parse(const std::string& input) {
    m_doc.clear();

    DeserializationError error = deserializeJson(
        m_doc, input.c_str(), input.size(), DeserializationOption::Filter(m_filter));

    if (error) {
        CC_LOG_ERROR("deserializeJson() failed: %s", error.c_str());
        return CURRENT_CONDITIONS_JSON_ERROR;
    }

    JsonObject properties = m_doc["properties"];
    if (properties.isNull()) {
        CC_LOG_ERROR("Missing properties object in JSON response");
        return CURRENT_CONDITIONS_DATA_MISSING;
    }

    const char* prop_rawMessage = properties["rawMessage"] | "No data";
    const char* prop_textDescription = properties["textDescription"] | "Unknown";
    raw_message = prop_rawMessage;
    description = prop_textDescription;

    JsonObject prop_temperature = properties["temperature"];
    if (!prop_temperature.isNull() && !prop_temperature["value"].isNull()) {
        temperature = prop_temperature["value"];
    } else {
        CC_LOG_WARNING("No valid temperature received");
        temperature = -999;
    }

    JsonObject prop_dewpoint = properties["dewpoint"];
    if (!prop_dewpoint.isNull() && !prop_dewpoint["value"].isNull()) {
        dew_point = prop_dewpoint["value"];
    } else {
        CC_LOG_WARNING("No valid dewpoint received");
        dew_point = -999;
    }

    JsonObject prop_windSpeed = properties["windSpeed"];
    if (!prop_windSpeed.isNull() && !prop_windSpeed["value"].isNull()) {
        wind_speed = prop_windSpeed["value"];
    } else {
        CC_LOG_WARNING("No valid wind speed received");
        wind_speed = -999;
    }

    if (temperature == -999 && dew_point == -999 && wind_speed == -999) {
        CC_LOG_ERROR("No valid weather data received");
        return CURRENT_CONDITIONS_DATA_MISSING;
    }

    CC_LOG_INFO("%s - Temp: %d C, Dew Point: %d C, Wind Speed: %d km/h",
                description, temperature, dew_point, wind_speed);

    return CURRENT_CONDITIONS_OK;
}

const char* CurrentConditions::getErrorString(int errorCode) {
    switch (errorCode) {
        case CURRENT_CONDITIONS_OK:            return "No error";
        case CURRENT_CONDITIONS_ERROR:         return "General error";
        case CURRENT_CONDITIONS_JSON_ERROR:    return "JSON parsing error";
        case CURRENT_CONDITIONS_NETWORK_ERROR: return "Network connection error";
        case CURRENT_CONDITIONS_DATA_MISSING:  return "Required data missing";
        case CURRENT_CONDITIONS_INVALID_DATA:  return "Invalid data values";
        case CURRENT_CONDITIONS_CERT_ERROR:    return "SSL certificate validation failed - check CA cert or run cert update";
        default:                               return "Unknown error";
    }
}
