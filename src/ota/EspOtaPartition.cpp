// ABOUTME: Writes a firmware image into the inactive OTA slot through ESP-IDF's esp_ota_ops.h.
// ABOUTME: Device-only implementation of IOtaPartition; never mixed with the Arduino Update library.
#include "EspOtaPartition.h"

#include <Arduino.h>

#include "../network/NetworkLog.h"

bool EspOtaPartition::begin() {
    m_target = esp_ota_get_next_update_partition(nullptr);
    if (m_target == nullptr) {
        NET_LOG_WARNING("No OTA update partition");
        return false;
    }
    // OTA_SIZE_UNKNOWN erases the whole slot, so the unsigned manifest size never decides what is erased.
    uint32_t start = millis();
    esp_err_t err = esp_ota_begin(m_target, OTA_SIZE_UNKNOWN, &m_handle);
    if (err != ESP_OK) {
        NET_LOG_WARNING("esp_ota_begin failed: %s", esp_err_to_name(err));
        return false;
    }
    NET_LOG_NOTICE("Erased OTA partition %s in %d ms", m_target->label, (int)(millis() - start));
    return true;
}

bool EspOtaPartition::write(const uint8_t* buf, size_t len) {
    esp_err_t err = esp_ota_write(m_handle, buf, len);
    if (err != ESP_OK) {
        NET_LOG_WARNING("esp_ota_write failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool EspOtaPartition::end() {
    // Validates the image header and checksum; frees the handle whether or not it succeeds.
    esp_err_t err = esp_ota_end(m_handle);
    if (err != ESP_OK) {
        NET_LOG_WARNING("esp_ota_end failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void EspOtaPartition::abort() { esp_ota_abort(m_handle); }

bool EspOtaPartition::setBoot() {
    esp_err_t err = esp_ota_set_boot_partition(m_target);
    if (err != ESP_OK) {
        NET_LOG_WARNING("esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
