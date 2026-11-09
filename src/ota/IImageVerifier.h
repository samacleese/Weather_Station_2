// ABOUTME: Interface for hashing a firmware image as it streams and checking it against the manifest.
// ABOUTME: verify() compares the SHA-256 and checks the ECDSA signature over it.
#ifndef I_IMAGE_VERIFIER_H
#define I_IMAGE_VERIFIER_H

#include <cstddef>
#include <cstdint>

class IImageVerifier {
   public:
    virtual ~IImageVerifier() {}
    virtual void begin() = 0;
    virtual void update(const uint8_t* buf, size_t len) = 0;
    virtual bool verify(const char* sha256Hex, const char* signatureB64) = 0;
};

#endif  // I_IMAGE_VERIFIER_H
