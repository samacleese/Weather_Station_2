// ABOUTME: Posts battery voltage readings to a local JSON store for discharge curve analysis.
// ABOUTME: Sends a single {ts, raw, adj} entry per reading; the server appends and manages the array.
#include "BatteryLogger.h"

#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <HTTPClient.h>

BatteryLogger::BatteryLogger(const String& url) : m_url(url) {}

void BatteryLogger::log(time_t timestamp, float rawVoltage, float adjustedVoltage) {
    StaticJsonDocument<96> doc;
    doc["ts"] = (long)timestamp;
    doc["raw"] = rawVoltage;
    doc["adj"] = adjustedVoltage;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(m_url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);

    if (httpCode != HTTP_CODE_CREATED) {
        Log.warning(F("[BatteryLogger] POST failed: %d" CR), httpCode);
    }

    http.end();
}
