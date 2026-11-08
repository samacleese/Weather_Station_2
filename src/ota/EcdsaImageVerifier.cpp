// ABOUTME: Hashes a streamed firmware image with SHA-256 and checks it against the signed manifest.
// ABOUTME: Device-only implementation of IImageVerifier; ECDSA P-256 over the digest with OTA_PUBLIC_KEY_DER.
#include "EcdsaImageVerifier.h"

#include <mbedtls/base64.h>
#include <mbedtls/pk.h>

#include <cstdlib>
#include <cstring>

#include "../network/NetworkLog.h"
#include "../security/OTAPublicKey.h"

static const size_t DIGEST_SIZE = 32;
static const size_t MAX_SIGNATURE_SIZE = 80;  // DER ECDSA P-256 signatures are at most 72 bytes

static bool fromHex(const char* hex, uint8_t* bytes, size_t len) {
    if (strlen(hex) != len * 2) return false;
    for (size_t i = 0; i < len; ++i) {
        char pair[3] = {hex[2 * i], hex[2 * i + 1], '\0'};
        char* end;
        bytes[i] = (uint8_t)strtoul(pair, &end, 16);
        if (*end != '\0') return false;
    }
    return true;
}

EcdsaImageVerifier::EcdsaImageVerifier() { mbedtls_sha256_init(&m_sha); }

EcdsaImageVerifier::~EcdsaImageVerifier() { mbedtls_sha256_free(&m_sha); }

void EcdsaImageVerifier::begin() {
    // An aborted install never reaches verify(), so release any previous context here. On the ESP32 a
    // hashing context can hold the hardware SHA engine until it is freed.
    mbedtls_sha256_free(&m_sha);
    mbedtls_sha256_init(&m_sha);
    mbedtls_sha256_starts(&m_sha, 0);  // 0 = SHA-256, not SHA-224
}

void EcdsaImageVerifier::update(const uint8_t* buf, size_t len) { mbedtls_sha256_update(&m_sha, buf, len); }

bool EcdsaImageVerifier::verify(const char* sha256Hex, const char* signatureB64) {
    uint8_t digest[DIGEST_SIZE];
    mbedtls_sha256_finish(&m_sha, digest);
    mbedtls_sha256_free(&m_sha);
    mbedtls_sha256_init(&m_sha);

    uint8_t expected[DIGEST_SIZE];
    if (!fromHex(sha256Hex, expected, DIGEST_SIZE)) {
        NET_LOG_WARNING("OTA manifest sha256 is not valid hex");
        return false;
    }
    if (memcmp(digest, expected, DIGEST_SIZE) != 0) {
        NET_LOG_WARNING("OTA image SHA-256 does not match the manifest");
        return false;
    }

    uint8_t signature[MAX_SIGNATURE_SIZE];
    size_t signatureLen = 0;
    int ret = mbedtls_base64_decode(
        signature, sizeof(signature), &signatureLen, (const uint8_t*)signatureB64, strlen(signatureB64));
    if (ret != 0) {
        NET_LOG_WARNING("OTA signature is not valid base64: %d", ret);
        return false;
    }

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    ret = mbedtls_pk_parse_public_key(&key, OTA_PUBLIC_KEY_DER, sizeof(OTA_PUBLIC_KEY_DER));
    if (ret == 0) ret = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, DIGEST_SIZE, signature, signatureLen);
    mbedtls_pk_free(&key);
    if (ret != 0) {
        NET_LOG_WARNING("OTA signature rejected: %d", ret);
        return false;
    }
    return true;
}
