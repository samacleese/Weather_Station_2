// ABOUTME: Weather.gov API client for fetching current conditions from a NWS station.
// ABOUTME: Parses observation JSON and exposes temperature, wind, dew point, and description.
#ifndef CURRENT_CONDITIONS_H
#define CURRENT_CONDITIONS_H

#include <ArduinoJson.h>
#include <string>

#include "IClock.h"
#include "IHttpClient.h"

const int CURRENT_CONDITIONS_OK = 0;
const int CURRENT_CONDITIONS_ERROR = -1;
const int CURRENT_CONDITIONS_JSON_ERROR = -2;
const int CURRENT_CONDITIONS_NETWORK_ERROR = -3;
const int CURRENT_CONDITIONS_DATA_MISSING = -4;
const int CURRENT_CONDITIONS_INVALID_DATA = -5;
const int CURRENT_CONDITIONS_CERT_ERROR = -6;

class CurrentConditions {
   public:
    CurrentConditions(IHttpClient& http, IClock& clock, const std::string& station);

    int update(int retries = 2);
    const char* getErrorString(int errorCode);
    bool validateData();

    const char* description = "Unknown";
    const char* raw_message = "No data";
    int temperature = -999;
    int dew_point = -999;
    int wind_speed = -999;
    int lastError = CURRENT_CONDITIONS_OK;

   private:
    int parse(const std::string& input);

    IHttpClient& m_http;
    IClock& m_clock;
    std::string m_url;
    DynamicJsonDocument m_doc;
    StaticJsonDocument<96> m_filter;

    struct {
        std::string description;
        std::string raw_message;
        int temperature = -999;
        int dew_point = -999;
        int wind_speed = -999;
        bool valid = false;
    } m_last_valid;

    unsigned long m_last_update_time = 0;
};

#endif  // CURRENT_CONDITIONS_H
