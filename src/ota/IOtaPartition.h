// ABOUTME: Interface for writing a firmware image into the inactive OTA slot and switching boot to it.
// ABOUTME: Mirrors esp_ota_ops.h; abort() is only valid before end(), which releases the handle either way.
#ifndef I_OTA_PARTITION_H
#define I_OTA_PARTITION_H

#include <cstddef>
#include <cstdint>

class IOtaPartition {
   public:
    virtual ~IOtaPartition() {}
    virtual bool begin() = 0;
    virtual bool write(const uint8_t* buf, size_t len) = 0;
    virtual bool end() = 0;
    virtual void abort() = 0;
    virtual bool setBoot() = 0;
};

#endif  // I_OTA_PARTITION_H
