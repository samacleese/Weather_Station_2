// ABOUTME: Throwaway OTA spikes C1 (#54) and C2 (#55): streams the published firmware in 4KB chunks while
// ABOUTME: hashing it, then checks the manifest's ECDSA P-256 signature against the embedded public key.
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "security/OTAPublicKey.h"
#include "weather_station_2/user_settings.h"

constexpr size_t CHUNK_SIZE = 4096;
constexpr size_t DIGEST_SIZE = 32;
constexpr size_t MAX_SIGNATURE_SIZE = 80;  // DER ECDSA P-256 signatures are at most 72 bytes
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;

static uint8_t chunk[CHUNK_SIZE];

struct Manifest {
    String url;
    size_t size;
    String sha256;
    String signature;  // base64 DER
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
    manifest.signature = doc["signature"].as<String>();
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

static bool fromHex(const String& hex, uint8_t* bytes, size_t len) {
    if (hex.length() != len * 2) return false;
    for (size_t i = 0; i < len; ++i) {
        char pair[3] = {hex[2 * i], hex[2 * i + 1], '\0'};
        char* end;
        bytes[i] = (uint8_t)strtoul(pair, &end, 16);
        if (*end != '\0') return false;
    }
    return true;
}

// Returns true when the full image arrived and its digest matches the manifest; fills `digest`.
static bool downloadAndHash(const Manifest& manifest, int run, uint8_t digest[DIGEST_SIZE]) {
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

    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    http.end();

    uint32_t elapsed = millis() - start;
    String hex = toHex(digest, DIGEST_SIZE);
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

// One signature case: verifies `digest` against `signature` and compares with the expected outcome.
static bool checkSignature(mbedtls_pk_context& key,
                           const char* name,
                           const uint8_t digest[DIGEST_SIZE],
                           const uint8_t* signature,
                           size_t signatureLen,
                           bool expectValid) {
    uint32_t start = micros();
    int ret = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, DIGEST_SIZE, signature, signatureLen);
    uint32_t elapsed = micros() - start;
    bool pass = expectValid ? ret == 0 : ret != 0;
    Serial.printf("sig case=\"%s\" ret=-0x%04x expected=%s verify_ms=%.1f RESULT %s\n",
                  name,
                  (unsigned)-ret,
                  expectValid ? "valid" : "rejected",
                  elapsed / 1000.0f,
                  pass ? "PASS" : "FAIL");
    return pass;
}

// Runs every signature case; returns the number that passed, or -1 if setup failed.
static int runSignatureCases(const Manifest& manifest, const uint8_t downloadedDigest[DIGEST_SIZE]) {
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    int ret = mbedtls_pk_parse_public_key(&key, OTA_PUBLIC_KEY_DER, sizeof(OTA_PUBLIC_KEY_DER));
    if (ret != 0 || mbedtls_pk_get_type(&key) != MBEDTLS_PK_ECKEY) {
        Serial.printf("Public key parse failed: ret=-0x%04x type=%d\n", (unsigned)-ret, mbedtls_pk_get_type(&key));
        mbedtls_pk_free(&key);
        return -1;
    }
    Serial.printf("Public key: %s, %u bits\n", mbedtls_pk_get_name(&key), (unsigned)mbedtls_pk_get_bitlen(&key));

    uint8_t signature[MAX_SIGNATURE_SIZE];
    size_t signatureLen = 0;
    ret = mbedtls_base64_decode(signature,
                                sizeof(signature),
                                &signatureLen,
                                (const uint8_t*)manifest.signature.c_str(),
                                manifest.signature.length());
    uint8_t manifestDigest[DIGEST_SIZE];
    if (ret != 0 || !fromHex(manifest.sha256, manifestDigest, DIGEST_SIZE)) {
        Serial.printf("Manifest signature/sha256 decode failed: ret=-0x%04x\n", (unsigned)-ret);
        mbedtls_pk_free(&key);
        return -1;
    }
    Serial.printf("Signature: %u bytes DER\n", (unsigned)signatureLen);

    uint8_t tamperedDigest[DIGEST_SIZE];
    memcpy(tamperedDigest, manifestDigest, DIGEST_SIZE);
    tamperedDigest[0] ^= 0x01;

    // The last byte sits inside the DER-encoded `s` integer, so flipping it keeps the encoding
    // parseable and exercises the actual signature math rather than the DER parser.
    uint8_t tamperedSignature[MAX_SIGNATURE_SIZE];
    memcpy(tamperedSignature, signature, signatureLen);
    tamperedSignature[signatureLen - 1] ^= 0x01;

    int passed = 0;
    passed += checkSignature(key, "manifest digest", manifestDigest, signature, signatureLen, true);
    passed += checkSignature(key, "downloaded digest", downloadedDigest, signature, signatureLen, true);
    passed += checkSignature(key, "tampered digest", tamperedDigest, signature, signatureLen, false);
    passed += checkSignature(key, "tampered signature", manifestDigest, tamperedSignature, signatureLen, false);
    mbedtls_pk_free(&key);
    return passed;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("SPIKE-BEGIN OtaUpdate");

    Manifest manifest;
    if (!connectWifi()) {
        Serial.println("WiFi connect timed out");
    } else if (fetchManifest(manifest)) {
        uint8_t downloadedDigest[DIGEST_SIZE];
        bool downloaded = downloadAndHash(manifest, 1, downloadedDigest);
        int signaturePassed = downloaded ? runSignatureCases(manifest, downloadedDigest) : -1;
        Serial.printf("SUMMARY download=%s signature_cases=%d/4\n", downloaded ? "PASS" : "FAIL", signaturePassed);
    }
    Serial.println("SPIKE-END");
}

void loop() {}
