// ABOUTME: Throwaway D1 (#58) spike: runs the real OTAManager::checkForUpdate against the live manifest over
// ABOUTME: plain HTTP, then one HTTPS weather fetch to confirm WeatherNetwork's TLS path still works.
#include <ArduinoLog.h>
#include <inttypes.h>

#include "src/network/CurrentConditions.h"
#include "src/network/Network.h"
#include "src/network/SystemClock.h"
#include "src/ota/OTAManager.h"
#include "weather_station_2/user_settings.h"
#include "weather_station_2/version.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    Serial.printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

void setup() {
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Serial.printf("\n=== OtaCheck spike, FIRMWARE_VERSION %" PRIu64 " ===\n", (uint64_t)FIRMWARE_VERSION);

    WeatherNetwork network(WIFI_SSID, WIFI_PASSWORD);
    network.begin();

    // currentVersion 0 forces the accept path, so every parsed field can be checked on the device.
    OTAManager anyVersion(network, 0);
    OTAManager::UpdateInfo info;
    bool found = anyVersion.checkForUpdate(OTA_MANIFEST_URL, info);
    check(found, "manifest fetched over plain HTTP and accepted against version 0");
    if (found) {
        Serial.printf("  version=%" PRIu64 " size=%u min_battery_mv=%d\n",
                      info.version,
                      (unsigned)info.size,
                      info.min_battery_mv);
        Serial.printf("  sha256=%s\n  signature=%s\n  url=%s\n", info.sha256, info.signature, info.url);
        // An 11-digit version only survives if ArduinoJson stores 64-bit integers on this target.
        check(info.version > 0xFFFFFFFFULL, "version parsed as 64-bit");
    }

    OTAManager running(network, FIRMWARE_VERSION);
    OTAManager::UpdateInfo runningInfo;
    bool newer = running.checkForUpdate(OTA_MANIFEST_URL, runningInfo);
    Serial.printf("  against FIRMWARE_VERSION: %s\n", newer ? "update available" : "no update");
    check(newer == (found && info.version > FIRMWARE_VERSION), "verdict against FIRMWARE_VERSION matches manifest");

    SystemClock clock;
    CurrentConditions weather(network, clock, "KBFI");
    check(weather.update() == CURRENT_CONDITIONS_OK, "HTTPS weather.gov fetch still works");

    Serial.printf("=== OtaCheck %s (%d failures) ===\n", failures == 0 ? "PASSED" : "FAILED", failures);
}

void loop() {}
