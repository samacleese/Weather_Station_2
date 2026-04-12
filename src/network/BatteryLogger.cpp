// ABOUTME: Posts battery voltage readings to a local JSON store for discharge curve analysis.
// ABOUTME: Appends {ts, raw, adj} entries to a server-side array capped at MAX_ENTRIES.
#include "BatteryLogger.h"

#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <HTTPClient.h>

// Enough capacity for MAX_ENTRIES objects with 3 numeric fields each, plus serialisation buffer.
static const size_t DOC_CAPACITY = 57344;  // 56 KB

BatteryLogger::BatteryLogger(const String& url) : m_url(url) {}

void BatteryLogger::log(time_t timestamp, float rawVoltage, float adjustedVoltage) {
    HTTPClient http;
    DynamicJsonDocument doc(DOC_CAPACITY);

    // Fetch existing array from the store
    http.begin(m_url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DeserializationError err = deserializeJson(doc, payload);
        if (err || !doc.is<JsonArray>()) {
            Log.warning(F("[BatteryLogger] Unreadable existing data, starting fresh" CR));
            doc.to<JsonArray>();
        }
    } else if (httpCode == HTTP_CODE_NOT_FOUND) {
        doc.to<JsonArray>();
    } else {
        Log.warning(F("[BatteryLogger] GET failed: %d" CR), httpCode);
        http.end();
        return;
    }
    http.end();

    JsonArray arr = doc.as<JsonArray>();

    // Drop oldest entries when at capacity
    while ((int)arr.size() >= MAX_ENTRIES) {
        arr.remove(0);
    }

    JsonObject entry = arr.createNestedObject();
    entry["ts"] = (long)timestamp;
    entry["raw"] = rawVoltage;
    entry["adj"] = adjustedVoltage;

    String body;
    serializeJson(doc, body);

    http.begin(m_url);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.PUT(body);

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_CREATED) {
        Log.warning(F("[BatteryLogger] PUT failed: %d" CR), httpCode);
    }

    http.end();
}
