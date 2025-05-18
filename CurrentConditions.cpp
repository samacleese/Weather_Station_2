// CurrentConditions.cpp - Improved error handling
#include "CurrentConditions.h"

#include <ArduinoLog.h>
#include <WiFiClientSecure.h>

#include <cstdio>

#include "CACerts.h"

static const char* url_format = "https://api.weather.gov/stations/%s/observations/latest";
static unsigned int json_size = 768;

CurrentConditions::CurrentConditions(std::shared_ptr<Network> network, const String station)
    : m_network(network), m_station(station), m_doc(json_size) {
    char buffer[256];
    sprintf(buffer, url_format, m_station.c_str());
    if (!m_url.setUrl(buffer)) {
        Log.error(F("Failure to set URL: %s" CR), buffer);
        lastError = CURRENT_CONDITIONS_ERROR;
    }

    JsonObject filter_properties = m_filter.createNestedObject("properties");
    filter_properties["textDescription"] = true;
    filter_properties["rawMessage"] = true;
    filter_properties["windSpeed"] = true;
    filter_properties["dewpoint"] = true;
    filter_properties["temperature"] = true;

    Log.notice(F("Set Current Conditions URL to %s" CR), m_url.getUrl().c_str());
}

int CurrentConditions::update(int retries) {
    WiFiClientSecure client;
    StreamString stream;

    // Reset last error
    lastError = CURRENT_CONDITIONS_OK;

    client.setCACert(CACerts::getCert(m_url.getHost()));

    Log.notice(F("Getting Current Conditions" CR));

    // Get data with retries
    int networkResult = m_network->get(client, m_url.getUrl(), stream, retries);

    if (networkResult != NETWORK_OK) {
        Log.error(F("Network error: %s" CR), m_network->getErrorString(networkResult));

        // Map network errors to our error codes
        switch (networkResult) {
            case NETWORK_WIFI_ERROR:
            case NETWORK_HTTP_ERROR:
            case NETWORK_TIMEOUT_ERROR:
            case NETWORK_NO_DATA:
                lastError = CURRENT_CONDITIONS_NETWORK_ERROR;
                break;
            default:
                lastError = CURRENT_CONDITIONS_ERROR;
        }

        // If we have previous valid data that's not too old (30 minutes), use it
        if (m_last_valid.valid && (millis() - m_last_update_time < 1800000)) {
            Log.warning(F("Using cached weather data from previous update" CR));
            description = m_last_valid.description.c_str();
            raw_message = m_last_valid.raw_message.c_str();
            temperature = m_last_valid.temperature;
            dew_point = m_last_valid.dew_point;
            wind_speed = m_last_valid.wind_speed;
            return lastError;  // Return error but we've set fallback data
        }

        return lastError;
    }

    // Check if we got any data
    if (stream.length() == 0) {
        Log.error(F("Empty response from server" CR));
        lastError = CURRENT_CONDITIONS_NETWORK_ERROR;
        return lastError;
    }

    // Parse the JSON data
    int parseResult = this->parse(stream);

    // Validate the data
    if (parseResult == CURRENT_CONDITIONS_OK && !validateData()) {
        parseResult = CURRENT_CONDITIONS_INVALID_DATA;
    }

    // If parsing succeeded, cache the valid data
    if (parseResult == CURRENT_CONDITIONS_OK) {
        m_last_valid.description = description;
        m_last_valid.raw_message = raw_message;
        m_last_valid.temperature = temperature;
        m_last_valid.dew_point = dew_point;
        m_last_valid.wind_speed = wind_speed;
        m_last_valid.valid = true;
        m_last_update_time = millis();
    }

    lastError = parseResult;
    return parseResult;
}

bool CurrentConditions::validateData() {
    // Check for valid temperature range (-100 to +100 should cover all realistic weather)
    if (temperature < -100 || temperature > 100) {
        Log.error(F("Invalid temperature value: %d" CR), temperature);
        return false;
    }

    // Check for valid dew point (should normally be lower than or equal to temperature)
    if (dew_point < -100 || dew_point > 100 || dew_point > temperature + 5) {
        Log.error(F("Invalid dew point value: %d" CR), dew_point);
        return false;
    }

    // Check for valid wind speed (0 to 500 km/h should cover all cases, even hurricanes)
    if (wind_speed < 0 || wind_speed > 500) {
        Log.error(F("Invalid wind speed value: %d" CR), wind_speed);
        return false;
    }

    return true;
}

int CurrentConditions::parse(Stream& input) {
    // Clear the document to prevent issues with previous data
    m_doc.clear();

    // Parse code generated with https://arduinojson.org/v6/assistant/
    DeserializationError error = deserializeJson(m_doc, input, DeserializationOption::Filter(m_filter));

    if (error) {
        Log.error(F("deserializeJson() failed: %s" CR), error.f_str());
        return CURRENT_CONDITIONS_JSON_ERROR;
    }

    // Check if the JSON structure is as expected
    JsonObject properties = m_doc["properties"];
    if (properties.isNull()) {
        Log.error(F("Missing properties object in JSON response" CR));
        return CURRENT_CONDITIONS_DATA_MISSING;
    }

    // Extract data, now with null checks
    const char* prop_rawMessage = properties["rawMessage"] | "No data";
    const char* prop_textDescription = properties["textDescription"] | "Unknown";

    raw_message = prop_rawMessage;
    description = prop_textDescription;

    // Check for temperature data
    JsonObject prop_temperature = properties["temperature"];
    if (!prop_temperature.isNull() && !prop_temperature["value"].isNull()) {
        temperature = prop_temperature["value"];
    } else {
        Log.warning(F("No valid temperature received" CR));
        temperature = -999;  // Error value
    }

    // Check for dewpoint data
    JsonObject prop_dewpoint = properties["dewpoint"];
    if (!prop_dewpoint.isNull() && !prop_dewpoint["value"].isNull()) {
        dew_point = prop_dewpoint["value"];
    } else {
        Log.warning(F("No valid dewpoint received" CR));
        dew_point = -999;  // Error value
    }

    // Check for wind speed data
    JsonObject prop_windSpeed = properties["windSpeed"];
    if (!prop_windSpeed.isNull() && !prop_windSpeed["value"].isNull()) {
        wind_speed = prop_windSpeed["value"];
    } else {
        Log.warning(F("No valid wind speed received" CR));
        wind_speed = -999;  // Error value
    }

    // Check if we got at least some valid data
    if (temperature == -999 && dew_point == -999 && wind_speed == -999) {
        Log.error(F("No valid weather data received" CR));
        return CURRENT_CONDITIONS_DATA_MISSING;
    }

    String output;
    serializeJson(properties, output);
    output.concat(CR);
    Log.info(output.c_str());
    Log.info(F("Raw Message %s" CR), raw_message);
    Log.info(
        F("%s - Temp: %d C, Dew Point: %d C, Wind Speed: %d km/h" CR), description, temperature, dew_point, wind_speed);

    return CURRENT_CONDITIONS_OK;
}

const char* CurrentConditions::getErrorString(int errorCode) {
    switch (errorCode) {
        case CURRENT_CONDITIONS_OK:
            return "No error";
        case CURRENT_CONDITIONS_ERROR:
            return "General error";
        case CURRENT_CONDITIONS_JSON_ERROR:
            return "JSON parsing error";
        case CURRENT_CONDITIONS_NETWORK_ERROR:
            return "Network connection error";
        case CURRENT_CONDITIONS_DATA_MISSING:
            return "Required data missing";
        case CURRENT_CONDITIONS_INVALID_DATA:
            return "Invalid data values";
        default:
            return "Unknown error";
    }
}
