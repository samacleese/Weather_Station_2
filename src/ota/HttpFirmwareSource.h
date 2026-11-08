// ABOUTME: Streams a firmware image over plain HTTP from the local json-store with a 30s read timeout.
// ABOUTME: Device-only implementation of IFirmwareSource; a stalled or dropped connection ends as a short read.
#ifndef HTTP_FIRMWARE_SOURCE_H
#define HTTP_FIRMWARE_SOURCE_H

#include <HTTPClient.h>

#include "IFirmwareSource.h"

class HttpFirmwareSource : public IFirmwareSource {
   public:
    bool open(const char* url) override;
    size_t read(uint8_t* buf, size_t len) override;
    void close() override;

   private:
    HTTPClient m_http;
    NetworkClient* m_stream = nullptr;
};

#endif  // HTTP_FIRMWARE_SOURCE_H
