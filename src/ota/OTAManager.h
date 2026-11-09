// ABOUTME: Checks the OTA manifest for firmware newer than the running build.
// ABOUTME: Fetches through IHttpClient and rejects any manifest with a missing or malformed field.
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <cstdint>

#include "../network/IHttpClient.h"

class OTAManager {
   public:
    struct UpdateInfo {
        uint64_t version;     // YYYYMMDDNNN -- exceeds uint32_t, must be 64-bit
        uint32_t size;        // bytes to download; the SHA-256 check covers it, so a wrong size fails verification
        char sha256[65];      // 64 hex chars + null
        char signature[200];  // base64 DER ECDSA signature (~100 chars; headroom)
        char url[128];
        int min_battery_mv;
    };

    OTAManager(IHttpClient& http, uint64_t currentVersion);

    // Fetches the manifest at manifestUrl. Returns true and fills info only when the manifest is
    // complete and well-formed and its version is newer than currentVersion; info is untouched otherwise.
    bool checkForUpdate(const char* manifestUrl, UpdateInfo& info);

   private:
    IHttpClient& m_http;
    uint64_t m_currentVersion;
};

#endif  // OTA_MANAGER_H
