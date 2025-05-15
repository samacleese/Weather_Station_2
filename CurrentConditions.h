#ifndef CURRENT_CONDITIONS_H
#define CURRENT_CONDITIONS_H
#include <ArduinoJson.h>
#include <LCBUrl.h>
#include <WString.h>

#include <memory>

#include "Network.h"

const int CURRENT_CONDITIONS_OK = 0;
const int CURRENT_CONDITIONS_ERROR = -1;

class CurrentConditions {
   public:
    CurrentConditions(std::shared_ptr<Network> network, const String station = "KBFI");
    int update();

    const char* description;
    const char* raw_message;
    int temperature;
    int dew_point;
    int wind_speed;

   private:
    int parse(Stream& input);
    JsonObject m_properties;
    std::shared_ptr<Network> m_network;
    String m_station;
    LCBUrl m_url;
    DynamicJsonDocument m_doc;
    StaticJsonDocument<96> m_filter;
};
#endif  // CURRENT_CONDITIONS_H
