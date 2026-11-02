// ABOUTME: Host unit tests for OTAManager::checkForUpdate manifest fetching and validation.
// ABOUTME: Feeds canned manifests through StubHttpClient — no WiFi required.
#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "StubHttpClient.h"
#include "ota/OTAManager.h"

static const char* MANIFEST_URL = "http://192.168.1.2:5000/ota-firmware/data";
static const uint64_t CURRENT_VERSION = 20260728001ULL;

// Copied from the live manifest published by tools/ota_publish.py, with the version bumped by one.
static const char* SHA256 = "a0594dd41ed47db85f5e604d0f888843942bd2055cd9e828b6e314102ed14182";
static const char* SIGNATURE =
    "MEUCICfe4BxnC12bksqLrOKRyKaOXFDcnTD2bALc/xdm+o7xAiEAiGdK+EOHgeBJc8ms4a5gVcG3VXvPoZsNL4N8fVvo/FU=";
static const char* BINARY_URL = "http://192.168.1.2:5000/ota-firmware/binaries/firmware";

static std::string validManifest() {
    return std::string("{\"version\": 20260728002, \"size\": 1757408, \"sha256\": \"") + SHA256 +
           "\", \"signature\": \"" + SIGNATURE + "\", \"url\": \"" + BINARY_URL + "\", \"min_battery_mv\": 3600}";
}

TEST(CheckForUpdateTest, NewerManifestPopulatesInfo) {
    StubHttpClient http;
    http.next_body = validManifest();
    OTAManager ota(http, CURRENT_VERSION);
    OTAManager::UpdateInfo info;

    ASSERT_TRUE(ota.checkForUpdate(MANIFEST_URL, info));

    EXPECT_EQ(http.last_url, MANIFEST_URL);
    EXPECT_EQ(info.version, 20260728002ULL);
    EXPECT_EQ(info.size, 1757408u);
    EXPECT_STREQ(info.sha256, SHA256);
    EXPECT_STREQ(info.signature, SIGNATURE);
    EXPECT_STREQ(info.url, BINARY_URL);
    EXPECT_EQ(info.min_battery_mv, 3600);
}

// ── Rejected manifests ───────────────────────────────────────────────────────

// Returns the valid manifest with one edit applied, so each rejection test changes exactly one thing.
template <typename Edit>
static std::string manifestWith(Edit edit) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, validManifest());
    JsonObject manifest = doc.as<JsonObject>();
    edit(manifest);
    std::string out;
    serializeJson(doc, out);
    return out;
}

// A rejected manifest must return false and leave the caller's info exactly as it was.
static void expectRejected(const std::string& manifestBody, int httpResult = 0) {
    StubHttpClient http;
    http.next_body = manifestBody;
    http.next_result = httpResult;
    OTAManager ota(http, CURRENT_VERSION);

    OTAManager::UpdateInfo info;
    memset(&info, 0xAB, sizeof(info));
    OTAManager::UpdateInfo before = info;

    EXPECT_FALSE(ota.checkForUpdate(MANIFEST_URL, info));
    EXPECT_EQ(memcmp(&info, &before, sizeof(info)), 0) << "info was modified on rejection";
}

TEST(CheckForUpdateTest, NetworkErrorRejected) { expectRejected(validManifest(), -1); }

TEST(CheckForUpdateTest, MalformedJsonRejected) { expectRejected("{\"version\": 20260728002, \"size\":"); }

TEST(CheckForUpdateTest, EqualVersionRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["version"] = CURRENT_VERSION; }));
}

TEST(CheckForUpdateTest, OlderVersionRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["version"] = CURRENT_VERSION - 1; }));
}

class MissingFieldTest : public ::testing::TestWithParam<const char*> {};

TEST_P(MissingFieldTest, Rejected) {
    const char* field = GetParam();
    expectRejected(manifestWith([field](JsonObject m) { m.remove(field); }));
}

INSTANTIATE_TEST_SUITE_P(EveryField,
                         MissingFieldTest,
                         ::testing::Values("version", "size", "sha256", "signature", "url", "min_battery_mv"));

class WrongTypeFieldTest : public ::testing::TestWithParam<const char*> {};

// Numbers sent as strings and strings sent as numbers; ArduinoJson would otherwise coerce them to 0 or null.
TEST_P(WrongTypeFieldTest, Rejected) {
    const char* field = GetParam();
    expectRejected(manifestWith([field](JsonObject m) {
        if (m[field].is<const char*>()) {
            m[field] = 12345;
        } else {
            m[field] = "12345";
        }
    }));
}

INSTANTIATE_TEST_SUITE_P(EveryField,
                         WrongTypeFieldTest,
                         ::testing::Values("version", "size", "sha256", "signature", "url", "min_battery_mv"));

TEST(CheckForUpdateTest, EmptySignatureRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["signature"] = ""; }));
}

TEST(CheckForUpdateTest, EmptyUrlRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["url"] = ""; }));
}

TEST(CheckForUpdateTest, ShortSha256Rejected) {
    expectRejected(manifestWith([](JsonObject m) { m["sha256"] = std::string(SHA256).substr(1); }));
}

TEST(CheckForUpdateTest, LongSha256Rejected) {
    expectRejected(manifestWith([](JsonObject m) { m["sha256"] = std::string(SHA256) + "0"; }));
}

// A truncated url or signature would fail confusingly later in performUpdate, so overlong values are rejected here.
TEST(CheckForUpdateTest, UrlTooLongForBufferRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["url"] = std::string(128, 'u'); }));
}

TEST(CheckForUpdateTest, SignatureTooLongForBufferRejected) {
    expectRejected(manifestWith([](JsonObject m) { m["signature"] = std::string(200, 's'); }));
}

TEST(CheckForUpdateTest, UrlFillingBufferExactlyAccepted) {
    StubHttpClient http;
    std::string url(127, 'u');
    http.next_body = manifestWith([&url](JsonObject m) { m["url"] = url; });
    OTAManager ota(http, CURRENT_VERSION);
    OTAManager::UpdateInfo info;

    ASSERT_TRUE(ota.checkForUpdate(MANIFEST_URL, info));
    EXPECT_EQ(std::string(info.url), url);
}

TEST(CheckForUpdateTest, SignatureFillingBufferExactlyAccepted) {
    StubHttpClient http;
    std::string signature(199, 's');
    http.next_body = manifestWith([&signature](JsonObject m) { m["signature"] = signature; });
    OTAManager ota(http, CURRENT_VERSION);
    OTAManager::UpdateInfo info;

    ASSERT_TRUE(ota.checkForUpdate(MANIFEST_URL, info));
    EXPECT_EQ(std::string(info.signature), signature);
}
