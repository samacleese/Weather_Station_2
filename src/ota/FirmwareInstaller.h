// ABOUTME: Streams a signed firmware image into the inactive OTA slot and makes it the boot target.
// ABOUTME: Holds only the sequencing; the download, flash and crypto sit behind three injected interfaces.
#ifndef FIRMWARE_INSTALLER_H
#define FIRMWARE_INSTALLER_H

#include <cstddef>

#include "IFirmwareSource.h"
#include "IImageVerifier.h"
#include "IOtaPartition.h"
#include "OTAManager.h"

class FirmwareInstaller {
   public:
    // Called every 64KB and once at the end. Must not be null.
    typedef void (*ProgressFn)(size_t received, size_t total);

    FirmwareInstaller(IFirmwareSource& source, IOtaPartition& partition, IImageVerifier& verifier);

    // Downloads info.size bytes from info.url into the inactive slot, verifies them against info.sha256 and
    // info.signature, then sets the slot as the boot target. Returns true when the next restart boots the
    // new image; the caller restarts. On false the running image still boots.
    // Uses one shared 4KB buffer, so only one install may run at a time.
    bool install(const OTAManager::UpdateInfo& info, ProgressFn progress);

   private:
    IFirmwareSource& m_source;
    IOtaPartition& m_partition;
    IImageVerifier& m_verifier;
};

#endif  // FIRMWARE_INSTALLER_H
