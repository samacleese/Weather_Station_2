// ABOUTME: Checks the OTA manifest for firmware newer than the running build.
// ABOUTME: Fetches through IHttpClient and rejects any manifest with a missing or malformed field.
#include "OTAManager.h"

#include <ArduinoJson.h>

#include <cstring>
#include <string>

#ifdef ARDUINO
#include <ArduinoLog.h>
#define OTA_LOG_WARNING(fmt, ...) Log.warningln(fmt, ##__VA_ARGS__)
#define OTA_LOG_NOTICE(fmt, ...) Log.noticeln(fmt, ##__VA_ARGS__)
#else
#define OTA_LOG_WARNING(fmt, ...)
#define OTA_LOG_NOTICE(fmt, ...)
#endif

static const size_t SHA256_HEX_LENGTH = 64;

// Copies a non-empty JSON string into dest. Fails rather than truncating when it does not fit,
// since a truncated url or signature would only surface later as a confusing download or verify failure.
static bool copyString(JsonVariantConst value, char* dest, size_t destSize) {
    if (!value.is<const char*>()) return false;
    const char* str = value.as<const char*>();
    size_t len = strlen(str);
    if (len == 0 || len >= destSize) return false;
    memcpy(dest, str, len + 1);
    return true;
}

OTAManager::OTAManager(IHttpClient& http, uint64_t currentVersion) : m_http(http), m_currentVersion(currentVersion) {}

bool OTAManager::checkForUpdate(const char* manifestUrl, UpdateInfo& info) {
    std::string body;
    int result = m_http.get(manifestUrl, body);
    if (result != 0) {
        OTA_LOG_WARNING("OTA manifest fetch failed: %d", result);
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        OTA_LOG_WARNING("OTA manifest is not valid JSON: %s", error.c_str());
        return false;
    }

    // ArduinoJson yields 0 or null for missing or mistyped fields, so every field is type-checked explicitly.
    if (!doc["version"].is<uint64_t>() || !doc["size"].is<uint32_t>() || !doc["min_battery_mv"].is<int>()) {
        OTA_LOG_WARNING("OTA manifest has a missing or non-integer version, size or min_battery_mv");
        return false;
    }

    UpdateInfo parsed;
    parsed.version = doc["version"].as<uint64_t>();
    parsed.size = doc["size"].as<uint32_t>();
    parsed.min_battery_mv = doc["min_battery_mv"].as<int>();

    if (!copyString(doc["sha256"], parsed.sha256, sizeof(parsed.sha256)) ||
        strlen(parsed.sha256) != SHA256_HEX_LENGTH) {
        OTA_LOG_WARNING("OTA manifest sha256 is missing or not %d hex chars", (int)SHA256_HEX_LENGTH);
        return false;
    }
    if (!copyString(doc["signature"], parsed.signature, sizeof(parsed.signature))) {
        OTA_LOG_WARNING("OTA manifest signature is missing, empty or too long");
        return false;
    }
    if (!copyString(doc["url"], parsed.url, sizeof(parsed.url))) {
        OTA_LOG_WARNING("OTA manifest url is missing, empty or too long");
        return false;
    }

    if (parsed.version <= m_currentVersion) {
        OTA_LOG_NOTICE("No firmware update available");
        return false;
    }

    info = parsed;
    return true;
}
