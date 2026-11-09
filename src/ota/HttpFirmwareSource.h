// ABOUTME: Streams a firmware image over plain HTTP from the local json-store, bounded by a 30s inactivity
// ABOUTME: timeout and a whole-download deadline. Device-only IFirmwareSource; a stall ends as a short read.
#ifndef HTTP_FIRMWARE_SOURCE_H
#define HTTP_FIRMWARE_SOURCE_H

#include <HTTPClient.h>

#include "IFirmwareSource.h"

class HttpFirmwareSource : public IFirmwareSource {
   public:
    // Bounds the body download, counted from open(). A normal download streams in about 8s; the default only
    // catches a server that trickles data. The response headers are bounded only by the inactivity timeout.
    static const uint32_t DEFAULT_DEADLINE_MS = 5 * 60 * 1000;

    explicit HttpFirmwareSource(uint32_t deadlineMs = DEFAULT_DEADLINE_MS) : m_deadlineMs(deadlineMs) {}
    bool open(const char* url) override;
    size_t read(uint8_t* buf, size_t len) override;
    void close() override;

   private:
    HTTPClient m_http;
    NetworkClient* m_stream = nullptr;
    uint32_t m_deadlineMs;
    uint32_t m_openedMs = 0;
};

#endif  // HTTP_FIRMWARE_SOURCE_H
