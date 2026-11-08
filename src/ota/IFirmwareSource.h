// ABOUTME: Interface for streaming a firmware image from wherever it is published.
// ABOUTME: FirmwareInstaller reads through it so the install sequence can be host-tested.
#ifndef I_FIRMWARE_SOURCE_H
#define I_FIRMWARE_SOURCE_H

#include <cstddef>
#include <cstdint>

class IFirmwareSource {
   public:
    virtual ~IFirmwareSource() {}
    // Starts the download. On failure the source cleans up after itself; close() is not needed.
    virtual bool open(const char* url) = 0;
    // Blocks until len bytes arrive or the read times out. Returns the bytes read; fewer than len means
    // the stream ended, dropped or stalled.
    virtual size_t read(uint8_t* buf, size_t len) = 0;
    virtual void close() = 0;
};

#endif  // I_FIRMWARE_SOURCE_H
