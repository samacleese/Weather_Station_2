// ABOUTME: Hashes a streamed firmware image with SHA-256 and checks it against the signed manifest.
// ABOUTME: Device-only implementation of IImageVerifier; ECDSA P-256 over the digest with OTA_PUBLIC_KEY_DER.
#ifndef ECDSA_IMAGE_VERIFIER_H
#define ECDSA_IMAGE_VERIFIER_H

#include <mbedtls/sha256.h>

#include "IImageVerifier.h"

class EcdsaImageVerifier : public IImageVerifier {
   public:
    EcdsaImageVerifier();
    ~EcdsaImageVerifier() override;
    void begin() override;
    void update(const uint8_t* buf, size_t len) override;
    bool verify(const char* sha256Hex, const char* signatureB64) override;

   private:
    mbedtls_sha256_context m_sha;
    bool m_hashFailed = false;
};

#endif  // ECDSA_IMAGE_VERIFIER_H
