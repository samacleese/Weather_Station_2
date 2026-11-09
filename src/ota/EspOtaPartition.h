// ABOUTME: Writes a firmware image into the inactive OTA slot through ESP-IDF's esp_ota_ops.h.
// ABOUTME: Device-only implementation of IOtaPartition; never mixed with the Arduino Update library.
#ifndef ESP_OTA_PARTITION_H
#define ESP_OTA_PARTITION_H

#include <esp_ota_ops.h>

#include "IOtaPartition.h"

class EspOtaPartition : public IOtaPartition {
   public:
    bool begin() override;
    bool write(const uint8_t* buf, size_t len) override;
    bool end() override;
    void abort() override;
    bool setBoot() override;

   private:
    const esp_partition_t* m_target = nullptr;
    esp_ota_handle_t m_handle = 0;
};

#endif  // ESP_OTA_PARTITION_H
