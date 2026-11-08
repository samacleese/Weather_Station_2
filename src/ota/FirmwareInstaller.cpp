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
    m_source.open(info.url);
    m_partition.begin();
    m_verifier.begin();

    size_t total = info.size;
    size_t received = 0;
    while (received < total) {
        size_t want = std::min(CHUNK_SIZE, total - received);
        size_t got = m_source.read(s_chunk, want);
        m_verifier.update(s_chunk, got);
        m_partition.write(s_chunk, got);
        received += got;
        if (received % PROGRESS_INTERVAL == 0 || received == total) progress(received, total);
    }

    m_source.close();
    m_verifier.verify(info.sha256, info.signature);
    m_partition.end();
    m_partition.setBoot();
    return true;
}
