// ABOUTME: Throwaway D2 (#59) spike: drives the real FirmwareInstaller and device implementations through
// ABOUTME: tampered, stalled and dropped installs, then one valid install that boots from the other slot.
#include <ArduinoLog.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <inttypes.h>

#include "OtaInstallSettings.h"
#include "src/network/Network.h"
#include "src/ota/EcdsaImageVerifier.h"
#include "src/ota/EspOtaPartition.h"
#include "src/ota/FirmwareInstaller.h"
#include "src/ota/HttpFirmwareSource.h"
#include "src/ota/OTAManager.h"
#include "weather_station_2/user_settings.h"
#include "weather_station_2/version.h"

static const size_t DROP_AT_BYTES = 512 * 1024;
static const uint32_t WIFI_TIMEOUT_MS = 20000;
// HttpFirmwareSource's read timeout is 30s; the window allows for scheduling slop.
static const uint32_t STALL_MIN_MS = 29000;
static const uint32_t STALL_MAX_MS = 35000;

static HttpFirmwareSource source;
static EspOtaPartition partition;
static EcdsaImageVerifier verifier;
static FirmwareInstaller installer(source, partition, verifier);

static int failures = 0;
static uint32_t lastProgressMs = 0;
static bool dropArmed = false;

static void check(bool ok, const char* what) {
    Serial.printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const char* bootLabel() { return esp_ota_get_boot_partition()->label; }

static void printProgress(size_t received, size_t total) {
    lastProgressMs = millis();
    Serial.printf("  progress %u / %u\n", (unsigned)received, (unsigned)total);
}

static void dropWifiMidStream(size_t received, size_t total) {
    printProgress(received, total);
    if (dropArmed && received >= DROP_AT_BYTES) {
        dropArmed = false;
        Serial.println("  dropping WiFi");
        WiFi.disconnect();
    }
}

static bool reconnectWifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) return false;
        delay(250);
    }
    return true;
}

// Runs one install that must fail and checks that the boot partition did not change.
static void expectRejectedInstall(const char* name,
                                  const OTAManager::UpdateInfo& info,
                                  FirmwareInstaller::ProgressFn progress) {
    Serial.printf("\n--- %s ---\n", name);
    const char* before = bootLabel();
    uint32_t start = millis();
    bool installed = installer.install(info, progress);
    Serial.printf("%s: install=%d elapsed_ms=%lu boot before=%s after=%s\n",
                  name,
                  installed,
                  (unsigned long)(millis() - start),
                  before,
                  bootLabel());
    check(!installed && strcmp(before, bootLabel()) == 0, name);
}

void setup() {
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Serial.printf("\n=== OtaInstall spike, FIRMWARE_VERSION %" PRIu64 ", running %s, boot %s ===\n",
                  (uint64_t)FIRMWARE_VERSION,
                  esp_ota_get_running_partition()->label,
                  bootLabel());

    WeatherNetwork network(WIFI_SSID, WIFI_PASSWORD);
    network.begin();

    // currentVersion 0 accepts whatever is published, so the spike always gets a full UpdateInfo.
    OTAManager anyVersion(network, 0);
    OTAManager::UpdateInfo published;
    if (!anyVersion.checkForUpdate(OTA_MANIFEST_URL, published)) {
        check(false, "manifest fetched");
        Serial.printf("=== OtaInstall FAILED (%d failures) ===\n", failures);
        return;
    }
    Serial.printf(
        "Published: version=%" PRIu64 " size=%u url=%s\n", published.version, (unsigned)published.size, published.url);

    OTAManager::UpdateInfo info = published;
    info.sha256[0] = info.sha256[0] == '0' ? '1' : '0';
    expectRejectedInstall("tampered sha256", info, printProgress);

    info = published;
    // Character 20 decodes into the DER r integer, so the signature still parses and the ECDSA math rejects it.
    info.signature[20] = info.signature[20] == 'A' ? 'B' : 'A';
    expectRejectedInstall("tampered signature", info, printProgress);

    static_assert(sizeof(OTA_STALL_URL) <= sizeof(info.url), "OTA_STALL_URL does not fit UpdateInfo::url");
    if (strlen(OTA_STALL_URL) == 0) {
        check(false, "stalled server (OTA_STALL_URL not set in config.cmake)");
    } else {
        info = published;
        strcpy(info.url, OTA_STALL_URL);
        expectRejectedInstall("stalled server", info, printProgress);
        uint32_t stallMs = millis() - lastProgressMs;
        Serial.printf("stalled server: gave up %lu ms after the last progress report\n", (unsigned long)stallMs);
        check(stallMs >= STALL_MIN_MS && stallMs <= STALL_MAX_MS, "stall ended by the 30s read timeout");
    }

    info = published;
    dropArmed = true;
    expectRejectedInstall("mid-stream WiFi drop", info, dropWifiMidStream);
    check(reconnectWifi(), "WiFi reconnected after drop");

    Serial.println("\n--- valid install ---");
    const char* running = esp_ota_get_running_partition()->label;
    uint32_t start = millis();
    bool installed = installer.install(published, printProgress);
    Serial.printf("valid install: install=%d elapsed_ms=%lu running=%s boot=%s\n",
                  installed,
                  (unsigned long)(millis() - start),
                  running,
                  bootLabel());
    check(installed && strcmp(running, bootLabel()) != 0, "valid install switched the boot partition");

    Serial.printf("=== OtaInstall %s (%d failures) ===\n", failures == 0 ? "PASSED" : "FAILED", failures);
    if (installed) {
        Serial.println("Restarting into the new image");
        Serial.flush();
        esp_restart();
    }
}

void loop() {}
