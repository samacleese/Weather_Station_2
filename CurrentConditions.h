// CurrentConditions.h - Improved error handling
#ifndef CURRENT_CONDITIONS_H
#define CURRENT_CONDITIONS_H
#include <ArduinoJson.h>
#include <LCBUrl.h>
#include <WString.h>

#include <memory>

#include "Network.h"

// Extended error codes
const int CURRENT_CONDITIONS_OK = 0;
const int CURRENT_CONDITIONS_ERROR = -1;
const int CURRENT_CONDITIONS_JSON_ERROR = -2;
const int CURRENT_CONDITIONS_NETWORK_ERROR = -3;
const int CURRENT_CONDITIONS_DATA_MISSING = -4;
const int CURRENT_CONDITIONS_INVALID_DATA = -5;

class CurrentConditions {
   public:
    CurrentConditions(std::shared_ptr<Network> network, const String station = "KBFI");

    // Returns error code, now with options for retries
    int update(int retries = 2);

    // Get the text representation of an error code
    const char* getErrorString(int errorCode);

    // Default values in case of errors
    const char* description = "Unknown";
    const char* raw_message = "No data";
    int temperature = -999;  // Use impossible values to indicate error state
    int dew_point = -999;
    int wind_speed = -999;

    // Last error code
    int lastError = CURRENT_CONDITIONS_OK;

   private:
    int parse(Stream& input);
    bool validateData();  // New method to validate data
    JsonObject m_properties;
    std::shared_ptr<Network> m_network;
    String m_station;
    LCBUrl m_url;
    DynamicJsonDocument m_doc;
    StaticJsonDocument<96> m_filter;

    // Store previous valid data as fallback
    struct {
        String description;
        String raw_message;
        int temperature;
        int dew_point;
        int wind_speed;
        bool valid = false;
    } m_last_valid;

    // Time of last successful update
    unsigned long m_last_update_time = 0;
};
#endif  // CURRENT_CONDITIONS_H
