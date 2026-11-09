// ABOUTME: Streams a signed firmware image into the inactive OTA slot and makes it the boot target.
// ABOUTME: Aborts the partition on any download or verification failure, so the running image keeps booting.
#include "FirmwareInstaller.h"

#include <algorithm>

#include "../network/NetworkLog.h"

static const size_t CHUNK_SIZE = 4096;
static const size_t PROGRESS_INTERVAL = 65536;

// File scope rather than a member or local: the Arduino loop task has an 8KB stack, and a member would
// put the buffer on it whenever the installer itself is a local.
static uint8_t s_chunk[CHUNK_SIZE];

FirmwareInstaller::FirmwareInstaller(IFirmwareSource& source, IOtaPartition& partition, IImageVerifier& verifier)
    : m_source(source), m_partition(partition), m_verifier(verifier) {}

bool FirmwareInstaller::install(const OTAManager::UpdateInfo& info, ProgressFn progress) {
    if (!m_source.open(info.url)) {
        NET_LOG_WARNING("OTA download could not start: %s", info.url);
        return false;
    }
    if (!m_partition.begin()) {
        NET_LOG_WARNING("OTA partition could not be prepared");
        m_source.close();
        return false;
    }
    m_verifier.begin();

    size_t total = info.size;
    size_t received = 0;
    while (received < total) {
        size_t want = std::min(CHUNK_SIZE, total - received);
        size_t got = m_source.read(s_chunk, want);
        if (got != want) {
            NET_LOG_WARNING("OTA download stopped at %d of %d bytes", (int)(received + got), (int)total);
            abortDownload();
            return false;
        }
        m_verifier.update(s_chunk, got);
        if (!m_partition.write(s_chunk, got)) {
            NET_LOG_WARNING("OTA flash write failed at %d bytes", (int)received);
            abortDownload();
            return false;
        }
        received += got;
        if (received % PROGRESS_INTERVAL == 0 || received == total) progress(received, total);
    }

    m_source.close();
    if (!m_verifier.verify(info.sha256, info.signature)) {
        NET_LOG_WARNING("OTA image failed verification; keeping the running firmware");
        m_partition.abort();
        return false;
    }
    // esp_ota_end releases the OTA handle even when it fails, so neither failure below may call abort().
    if (!m_partition.end()) {
        NET_LOG_WARNING("OTA image was rejected when finalizing the partition");
        return false;
    }
    if (!m_partition.setBoot()) {
        NET_LOG_WARNING("OTA boot partition could not be switched");
        return false;
    }
    NET_LOG_NOTICE("OTA image installed; it boots on the next restart");
    return true;
}

void FirmwareInstaller::abortDownload() {
    m_partition.abort();
    m_source.close();
}
