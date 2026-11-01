// ABOUTME: Throwaway OTA spike C1 (#54): streams the published firmware over plain HTTP in 4KB chunks,
// ABOUTME: hashing with SHA-256 as it goes, and reports digest, timing, and heap against the manifest.
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

#include "weather_station_2/user_settings.h"

constexpr size_t CHUNK_SIZE = 4096;
constexpr int RUNS = 3;
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;

static uint8_t chunk[CHUNK_SIZE];

struct Manifest {
    String url;
    size_t size;
    String sha256;
};

static bool connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) return false;
        delay(250);
    }
    Serial.printf("WiFi connected in %lu ms, RSSI %d dBm\n", (unsigned long)(millis() - start), WiFi.RSSI());
    return true;
}

static bool fetchManifest(Manifest& manifest) {
    HTTPClient http;
    http.begin(OTA_MANIFEST_URL);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Manifest GET failed: %d\n", code);
        http.end();
        return false;
    }
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) {
        Serial.printf("Manifest parse failed: %s\n", err.c_str());
        return false;
    }
    manifest.url = doc["url"].as<String>();
    manifest.size = doc["size"].as<size_t>();
    manifest.sha256 = doc["sha256"].as<String>();
    Serial.printf(
        "Manifest: url=%s size=%u sha256=%s\n", manifest.url.c_str(), (unsigned)manifest.size, manifest.sha256.c_str());
    return true;
}

static String toHex(const uint8_t* bytes, size_t len) {
    static const char digits[] = "0123456789abcdef";
    String hex;
    hex.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        hex += digits[bytes[i] >> 4];
        hex += digits[bytes[i] & 0x0f];
    }
    return hex;
}

// Returns true when the full image arrived and its digest matches the manifest.
static bool downloadAndHash(const Manifest& manifest, int run) {
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t start = millis();

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(manifest.url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("run=%d Firmware GET failed: %d\n", run, code);
        http.end();
        return false;
    }
    int contentLength = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);  // 0 = SHA-256, not SHA-224

    size_t received = 0;
    uint32_t minHeap = ESP.getFreeHeap();
    while (received < manifest.size) {
        size_t want = min(CHUNK_SIZE, manifest.size - received);
        // readBytes blocks until it has `want` bytes or the stream's timeout expires.
        size_t got = stream->readBytes(chunk, want);
        mbedtls_sha256_update(&sha, chunk, got);
        received += got;
        minHeap = min(minHeap, ESP.getFreeHeap());
        if (got < want) {
            Serial.printf("run=%d Short read at %u bytes (wanted %u, got %u); connected=%d\n",
                          run,
                          (unsigned)received,
                          (unsigned)want,
                          (unsigned)got,
                          stream->connected());
            break;
        }
    }

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    http.end();

    uint32_t elapsed = millis() - start;
    String hex = toHex(digest, sizeof(digest));
    bool complete = received == manifest.size;
    bool match = complete && hex.equalsIgnoreCase(manifest.sha256);

    Serial.printf("run=%d content_length=%d received=%u elapsed_ms=%lu rate_kBps=%.1f\n",
                  run,
                  contentLength,
                  (unsigned)received,
                  (unsigned long)elapsed,
                  elapsed ? received / (float)elapsed : 0.0f);
    Serial.printf("run=%d heap_before=%u heap_min_during=%u heap_after=%u largest_block=%u\n",
                  run,
                  (unsigned)heapBefore,
                  (unsigned)minHeap,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
    Serial.printf("run=%d sha256=%s\n", run, hex.c_str());
    Serial.printf("run=%d RESULT %s\n", run, match ? "PASS" : "FAIL");
    return match;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("SPIKE-BEGIN OtaDownload");

    Manifest manifest;
    if (!connectWifi()) {
        Serial.println("WiFi connect timed out");
    } else if (fetchManifest(manifest)) {
        int passed = 0;
        for (int run = 1; run <= RUNS; ++run) {
            if (downloadAndHash(manifest, run)) ++passed;
        }
        Serial.printf("SUMMARY %d/%d runs passed\n", passed, RUNS);
    }
    Serial.println("SPIKE-END");
}

void loop() {}
