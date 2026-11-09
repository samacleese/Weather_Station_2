# D2 FirmwareInstaller Implementation Plan (#59)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `FirmwareInstaller::install()` streams a signed firmware image into the inactive OTA slot and makes it the boot target, and it refuses on any download, flash or verification failure.

**Architecture:** `FirmwareInstaller` holds only the sequencing. It is host-compiled and host-tested against three interfaces (`IFirmwareSource`, `IOtaPartition`, `IImageVerifier`). Three thin device-only implementations wrap HTTPClient, `esp_ota_ops.h` and mbedtls. A hardware spike runs them for real.

**Tech Stack:** C++11, GoogleTest (host), arduino-cli with `soldered-inkplate-boards:esp32@3.0.0` (IDF 5.5), ESP-IDF `esp_ota_ops.h`, mbedtls SHA-256, ECDSA and base64.

**Spec:** `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md`, section "D2. `FirmwareInstaller::install`", especially "Design (agreed 2026-11-01)".

## Global Constraints

- ESP-IDF `esp_ota_ops.h` only. Never include or call the Arduino `Update` library.
- `esp_ota_begin(target, OTA_SIZE_UNKNOWN, &handle)`. The manifest `size` never decides what gets erased.
- `esp_ota_abort()` is called only before `esp_ota_end()`. After a failed `end()` or `setBoot()`, return false without aborting (IDF 5.5: `esp_ota_end()` frees the handle "regardless of result").
- `install()` returns true after `setBoot()`. It never restarts. The caller (D3) restarts.
- HTTP read timeout is 30 s (`HTTPClient::setTimeout(30000)`).
- 4 KB read/write chunks. Progress is reported each time `received` lands on a 64 KB multiple, and once at the end.
- No 4 KB array on the stack, since the Arduino loop task has an 8 KB stack.
- Every code file starts with two `// ABOUTME: ` lines (`# ABOUTME: ` for Python and CMake).
- Cross-directory includes in `src/` are relative (`"../network/IHttpClient.h"`). Host tests include `"ota/..."` through the `src` include dir.
- Device logging uses ArduinoLog with only `%s` and `%d` (cast sizes to `int`). Code in `src/ota/` logs through `NET_LOG_*` from `src/network/NetworkLog.h` (no-ops on host).
- Run `clang-format -i` only on files created or modified here.
- Commits use the conventional format and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Stage files explicitly and never stage `.worktrees/`.
- Work on branch `feat/issue-59-firmware-installer`, which already exists and holds the design commit 751496f.

## One deviation from the agreed design, for Jesse to confirm in review

The roadmap says the 4 KB chunk buffer is a `FirmwareInstaller` member. That makes the object itself 4 KB, so a `FirmwareInstaller installer(...)` declared in `setup()` (the obvious D3 code) would put the buffer right back on the 8 KB stack. This plan makes it a file-scope `static` in `FirmwareInstaller.cpp` instead, so the object is a few pointers and can live anywhere. The cost is that only one install can run at a time, which is always true on this device. Task 5 updates the roadmap sentence to match.

## File map

| File | Status | Responsibility |
|---|---|---|
| `src/ota/OTAManager.cpp` | modify | use master's `src/network/NetworkLog.h` instead of its own macro copy |
| `src/ota/OTAManager.h` | modify | `size` comment: it is now the download length |
| `src/ota/IFirmwareSource.h` | create | interface: `open`, `read`, `close` |
| `src/ota/IOtaPartition.h` | create | interface: `begin`, `write`, `end`, `abort`, `setBoot` |
| `src/ota/IImageVerifier.h` | create | interface: `begin`, `update`, `verify` |
| `src/ota/FirmwareInstaller.h/.cpp` | create | the `install()` sequence |
| `tests/unit/FirmwareInstallerTest.cpp` | create | host tests with logging fakes |
| `tests/unit/CMakeLists.txt` | modify | add the test file and `FirmwareInstaller.cpp` |
| `src/ota/HttpFirmwareSource.h/.cpp` | create | device: HTTPClient stream, 30 s timeout |
| `src/ota/EspOtaPartition.h/.cpp` | create | device: `esp_ota_ops.h` |
| `src/ota/EcdsaImageVerifier.h/.cpp` | create | device: mbedtls SHA-256 plus ECDSA verify with `OTA_PUBLIC_KEY_DER` |
| `tests/spikes/OtaCheck/src/network/{NetworkLog,NetworkResult}.h`, same in `tests/device/DeviceUnitTests/src/network/` | create (symlinks) | repairs the mirrors master broke |
| `tests/spikes/OtaInstall/` | create | hardware spike: `.ino`, `CMakeLists.txt`, `OtaInstallSettings.h.in`, `stall_server.py`, `src/` symlink mirror |
| `tests/spikes/CMakeLists.txt` | modify | `add_subdirectory(OtaInstall)` |
| `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md` | modify | story index ✅, buffer sentence, measured timings |

The main firmware build compiles everything under `src/`, so the device classes get compiled (and dead-stripped) in `./build.sh build` before D3 uses them. That build is their compile check.

---

### Task 1: Interfaces, shared log macros, and the install happy path

**Files:**
- Create: `src/ota/IFirmwareSource.h`, `src/ota/IOtaPartition.h`, `src/ota/IImageVerifier.h`, `src/ota/FirmwareInstaller.h`, `src/ota/FirmwareInstaller.cpp`, `tests/unit/FirmwareInstallerTest.cpp`
- Create symlinks: `NetworkLog.h` and `NetworkResult.h` in `tests/spikes/OtaCheck/src/network/` and `tests/device/DeviceUnitTests/src/network/`
- Modify: `src/ota/OTAManager.cpp:10-17` (macro block), `src/ota/OTAManager.h` (`size` comment), `tests/unit/CMakeLists.txt` (add_executable list)

**Interfaces:**
- Consumes: `OTAManager::UpdateInfo` from `src/ota/OTAManager.h` (`uint32_t size; char sha256[65]; char signature[200]; char url[128];`)
- Produces:
  - `class IFirmwareSource { virtual bool open(const char* url) = 0; virtual size_t read(uint8_t* buf, size_t len) = 0; virtual void close() = 0; }`
  - `class IOtaPartition { virtual bool begin() = 0; virtual bool write(const uint8_t* buf, size_t len) = 0; virtual bool end() = 0; virtual void abort() = 0; virtual bool setBoot() = 0; }`
  - `class IImageVerifier { virtual void begin() = 0; virtual void update(const uint8_t* buf, size_t len) = 0; virtual bool verify(const char* sha256Hex, const char* signatureB64) = 0; }`
  - `FirmwareInstaller(IFirmwareSource&, IOtaPartition&, IImageVerifier&)`, `typedef void (*ProgressFn)(size_t received, size_t total);`, `bool install(const OTAManager::UpdateInfo& info, ProgressFn progress);`
  - Consumes `NET_LOG_WARNING(fmt, ...)`, `NET_LOG_NOTICE(fmt, ...)` from `src/network/NetworkLog.h` (already on master)

- [ ] **Step 1: Share master's `NetworkLog.h` macros and repair the spike mirrors**

Master already has `src/network/NetworkLog.h` (the `NET_LOG_*` macros: ArduinoLog on the device, no-ops on the host). `OTAManager.cpp` has its own copy of that pattern, so consolidate it. In `src/ota/OTAManager.cpp`, replace the whole `#ifdef ARDUINO ... #endif` block (lines 10-17) with `#include "../network/NetworkLog.h"`, placed after `#include <string>`. Then rename `NET_LOG_WARNING` to `NET_LOG_WARNING` and `NET_LOG_NOTICE` to `NET_LOG_NOTICE` in that file.

The spike and device-test mirrors are missing the two headers master's `CurrentConditions.cpp` now includes, so OtaCheck no longer compiles (`fatal error: NetworkLog.h: No such file or directory`). Add them:

```bash
for d in tests/spikes/OtaCheck tests/device/DeviceUnitTests; do
    for f in NetworkLog.h NetworkResult.h; do ln -s ../../../../../src/network/$f $d/src/network/$f; done
done
```

In `src/ota/OTAManager.h`, change the `size` comment to:
`uint32_t size;  // bytes to download; the SHA-256 check covers it, so a wrong size fails verification`

Run `./build.sh test-host`. Expected: all 80 existing tests pass. This is a pure refactor, so no new test. Run `./build.sh build-spike OtaCheck` and expect it to compile. DeviceUnitTests still fails to compile because of the unrelated #78 (AUnit `test` macro), so don't use it as a check.

- [ ] **Step 2: Write the three interfaces**

`src/ota/IFirmwareSource.h`:
```cpp
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
```

`src/ota/IOtaPartition.h`:
```cpp
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
```

`src/ota/IImageVerifier.h`:
```cpp
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
```

- [ ] **Step 3: Write the failing happy-path tests**

Create `tests/unit/FirmwareInstallerTest.cpp`:

```cpp
// ABOUTME: Host unit tests for FirmwareInstaller::install sequencing and failure handling.
// ABOUTME: Fakes for the source, partition and verifier record calls in one shared log, so order is checked.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "ota/FirmwareInstaller.h"

typedef std::vector<std::string> CallLog;

static const size_t CHUNK_SIZE = 4096;
// Not a multiple of 4KB, so the last chunk is partial: 48 full chunks plus 3392 bytes.
static const size_t IMAGE_SIZE = 200000;

// Each byte depends on its offset and its chunk, so dropped, repeated or reordered chunks change the bytes.
static std::vector<uint8_t> makeImage(size_t size) {
    std::vector<uint8_t> image(size);
    for (size_t i = 0; i < size; ++i) image[i] = (uint8_t)(i * 7 + i / CHUNK_SIZE);
    return image;
}

// Data calls (read, write, update) are tracked as bytes, not log lines, so the log stays a readable sequence.
class FakeSource : public IFirmwareSource {
   public:
    explicit FakeSource(CallLog& log) : m_log(log) {}
    std::vector<uint8_t> image;  // what the server sends; shorter than info.size simulates a drop
    bool open_ok = true;
    std::string opened_url;
    int read_count = 0;

    bool open(const char* url) override {
        m_log.push_back("source.open");
        opened_url = url;
        return open_ok;
    }
    size_t read(uint8_t* buf, size_t len) override {
        read_count++;
        size_t n = std::min(len, image.size() - m_pos);
        memcpy(buf, image.data() + m_pos, n);
        m_pos += n;
        return n;
    }
    void close() override { m_log.push_back("source.close"); }

   private:
    CallLog& m_log;
    size_t m_pos = 0;
};

class FakePartition : public IOtaPartition {
   public:
    explicit FakePartition(CallLog& log) : m_log(log) {}
    bool begin_ok = true;
    bool end_ok = true;
    bool set_boot_ok = true;
    int fail_write_number = 0;  // 1-based write call that fails; 0 never fails
    std::vector<uint8_t> written;
    std::vector<size_t> write_sizes;

    bool begin() override {
        m_log.push_back("partition.begin");
        return begin_ok;
    }
    bool write(const uint8_t* buf, size_t len) override {
        write_sizes.push_back(len);
        if ((int)write_sizes.size() == fail_write_number) return false;
        written.insert(written.end(), buf, buf + len);
        return true;
    }
    bool end() override {
        m_log.push_back("partition.end");
        return end_ok;
    }
    void abort() override { m_log.push_back("partition.abort"); }
    bool setBoot() override {
        m_log.push_back("partition.setBoot");
        return set_boot_ok;
    }

   private:
    CallLog& m_log;
};

class FakeVerifier : public IImageVerifier {
   public:
    explicit FakeVerifier(CallLog& log) : m_log(log) {}
    bool verify_ok = true;
    std::vector<uint8_t> hashed;
    std::string verified_sha256;
    std::string verified_signature;

    void begin() override { m_log.push_back("verifier.begin"); }
    void update(const uint8_t* buf, size_t len) override { hashed.insert(hashed.end(), buf, buf + len); }
    bool verify(const char* sha256Hex, const char* signatureB64) override {
        m_log.push_back("verifier.verify");
        verified_sha256 = sha256Hex;
        verified_signature = signatureB64;
        return verify_ok;
    }

   private:
    CallLog& m_log;
};

// ProgressFn is a plain function pointer, so progress calls land in a file-scope list the fixture resets.
static std::vector<std::pair<size_t, size_t>> g_progress;
static void recordProgress(size_t received, size_t total) { g_progress.push_back(std::make_pair(received, total)); }

class FirmwareInstallerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        g_progress.clear();
        source.image = makeImage(IMAGE_SIZE);
        memset(&info, 0, sizeof(info));
        info.version = 20260728002ULL;
        info.size = IMAGE_SIZE;
        strcpy(info.sha256, "a0594dd41ed47db85f5e604d0f888843942bd2055cd9e828b6e314102ed14182");
        strcpy(info.signature, "MEUCICfe4BxnC12bksqLrOKRyKaOXFDcnTD2bALc/xdm+o7xAiEAiGdK+EOHgeBJc8ms4a5gVcG3VXvPoZsNL4N8fVvo/FU=");
        strcpy(info.url, "http://192.168.1.2:5000/ota-firmware/binaries/firmware");
        info.min_battery_mv = 3600;
    }

    bool install() { return installer.install(info, recordProgress); }

    CallLog log;
    FakeSource source{log};
    FakePartition partition{log};
    FakeVerifier verifier{log};
    FirmwareInstaller installer{source, partition, verifier};
    OTAManager::UpdateInfo info;
};

// ── Successful install ───────────────────────────────────────────────────────

TEST_F(FirmwareInstallerTest, SucceedsAndCallsEveryStepInOrder) {
    ASSERT_TRUE(install());

    CallLog expected = {"source.open",
                        "partition.begin",
                        "verifier.begin",
                        "source.close",
                        "verifier.verify",
                        "partition.end",
                        "partition.setBoot"};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(source.opened_url, info.url);
    EXPECT_EQ(verifier.verified_sha256, info.sha256);
    EXPECT_EQ(verifier.verified_signature, info.signature);
}

TEST_F(FirmwareInstallerTest, StreamsEveryByteToBothPartitionAndVerifier) {
    ASSERT_TRUE(install());

    EXPECT_EQ(partition.written, source.image);
    EXPECT_EQ(verifier.hashed, source.image);
}

TEST_F(FirmwareInstallerTest, WritesFull4KBChunksThenThePartialRemainder) {
    ASSERT_TRUE(install());

    ASSERT_EQ(partition.write_sizes.size(), 49u);
    for (size_t i = 0; i < 48; ++i) EXPECT_EQ(partition.write_sizes[i], CHUNK_SIZE) << "chunk " << i;
    EXPECT_EQ(partition.write_sizes[48], IMAGE_SIZE % CHUNK_SIZE);
}

TEST_F(FirmwareInstallerTest, ReadsExactlyManifestSizeWhenServerSendsMore) {
    source.image = makeImage(IMAGE_SIZE + 10000);

    ASSERT_TRUE(install());

    std::vector<uint8_t> expected(source.image.begin(), source.image.begin() + IMAGE_SIZE);
    EXPECT_EQ(partition.written, expected);
    EXPECT_EQ(verifier.hashed, expected);
}

TEST_F(FirmwareInstallerTest, ReportsProgressAt64KBMarksAndAtTheEnd) {
    ASSERT_TRUE(install());

    std::vector<std::pair<size_t, size_t>> expected = {
        {65536, IMAGE_SIZE}, {131072, IMAGE_SIZE}, {196608, IMAGE_SIZE}, {IMAGE_SIZE, IMAGE_SIZE}};
    EXPECT_EQ(g_progress, expected);
}

TEST_F(FirmwareInstallerTest, ReportsFinalProgressOnceWhenSizeIsA64KBMultiple) {
    info.size = 131072;
    source.image = makeImage(131072);

    ASSERT_TRUE(install());

    std::vector<std::pair<size_t, size_t>> expected = {{65536, 131072}, {131072, 131072}};
    EXPECT_EQ(g_progress, expected);
}
```

Add both files to `tests/unit/CMakeLists.txt`, keeping the list's existing order (tests first, then sources):

```cmake
add_executable(unit_tests
    CACertsTest.cpp
    CurrentConditionsTest.cpp
    NetworkResultTest.cpp
    PrecipForecastTest.cpp
    BatteryMeterTest.cpp
    OTAManagerTest.cpp
    FirmwareInstallerTest.cpp
    ../../src/security/CACerts.cpp
    ../../src/network/CurrentConditions.cpp
    ../../src/network/PrecipForecast.cpp
    ../../src/display/BatteryMeter.cpp
    ../../src/ota/OTAManager.cpp
    ../../src/ota/FirmwareInstaller.cpp
)
```

- [ ] **Step 4: Run the tests and watch them fail**

Run: `./build.sh test-host`
Expected: the build fails with `ota/FirmwareInstaller.h: No such file or directory`.

- [ ] **Step 5: Write the minimal installer**

`src/ota/FirmwareInstaller.h`:
```cpp
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
```

`src/ota/FirmwareInstaller.cpp`, minimal version (Task 2 adds every failure check):
```cpp
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
```

- [ ] **Step 6: Run the tests and watch them pass**

Run: `./build.sh test-host`
Expected: 86 tests pass (80 existing plus 6 new) with no other output.

- [ ] **Step 7: Format and commit**

```bash
clang-format -i src/ota/OTAManager.cpp src/ota/OTAManager.h src/ota/IFirmwareSource.h \
    src/ota/IOtaPartition.h src/ota/IImageVerifier.h src/ota/FirmwareInstaller.h src/ota/FirmwareInstaller.cpp \
    tests/unit/FirmwareInstallerTest.cpp
./build.sh test-host
git add src/ota/OTAManager.cpp src/ota/OTAManager.h src/ota/IFirmwareSource.h \
    src/ota/IOtaPartition.h src/ota/IImageVerifier.h src/ota/FirmwareInstaller.h src/ota/FirmwareInstaller.cpp \
    tests/unit/FirmwareInstallerTest.cpp tests/unit/CMakeLists.txt \
    tests/spikes/OtaCheck/src/network/NetworkLog.h tests/spikes/OtaCheck/src/network/NetworkResult.h \
    tests/device/DeviceUnitTests/src/network/NetworkLog.h tests/device/DeviceUnitTests/src/network/NetworkResult.h
git commit -m "feat(ota): add FirmwareInstaller install sequence (#59)" -m "..."
```

The commit body says that the success path streams through three interfaces so it can be host-tested, that OTAManager now uses master's NetworkLog.h instead of its own macro copy, that the spike and device-test mirrors gained the headers master's CurrentConditions.cpp needs, and that the chunk buffer is file-scope static (the deviation from the roadmap).

---

### Task 2: Install failure handling

**Files:**
- Modify: `tests/unit/FirmwareInstallerTest.cpp` (append tests), `src/ota/FirmwareInstaller.cpp` (`install`)

**Interfaces:**
- Consumes: everything Task 1 produced
- Produces: no new names. `install()` returns false on every failure path, with the call order the tests pin.

Do the seven tests one at a time. For each: append it, run `./build.sh test-host`, watch that one test fail, add the check from Step 3 that fixes it, and watch everything pass. Every test below fails against the Task 1 implementation (the comment on each says why).

- [ ] **Step 1: Write the failure tests**

```cpp
// ── Failed installs ──────────────────────────────────────────────────────────

// Red against Task 1: it carries on to begin, writes and setBoot.
TEST_F(FirmwareInstallerTest, OpenFailureLeavesPartitionUntouched) {
    source.open_ok = false;

    EXPECT_FALSE(install());

    EXPECT_EQ(log, CallLog({"source.open"}));
    EXPECT_TRUE(partition.write_sizes.empty());
}

// Red against Task 1: it reads and writes anyway.
TEST_F(FirmwareInstallerTest, BeginFailureClosesSourceWithoutReading) {
    partition.begin_ok = false;

    EXPECT_FALSE(install());

    EXPECT_EQ(log, CallLog({"source.open", "partition.begin", "source.close"}));
    EXPECT_EQ(source.read_count, 0);
}

// A server that sends less than info.size (dropped WiFi, stalled server hitting the read timeout).
// Red against Task 1: it loops forever on zero-byte reads, so implement this check before running the
// full suite (or the test binary hangs rather than failing).
TEST_F(FirmwareInstallerTest, ShortReadAbortsWithoutEndingOrSettingBoot) {
    source.image = makeImage(100000);

    EXPECT_FALSE(install());

    EXPECT_EQ(log, CallLog({"source.open", "partition.begin", "verifier.begin", "partition.abort", "source.close"}));
    // Progress stopped at the last 64KB mark reached; no final report for an incomplete image.
    std::vector<std::pair<size_t, size_t>> expected = {{65536, IMAGE_SIZE}};
    EXPECT_EQ(g_progress, expected);
}

// Red against Task 1: it keeps reading and goes on to end and setBoot.
TEST_F(FirmwareInstallerTest, WriteFailureAbortsAndStopsReading) {
    partition.fail_write_number = 3;

    EXPECT_FALSE(install());

    EXPECT_EQ(log, CallLog({"source.open", "partition.begin", "verifier.begin", "partition.abort", "source.close"}));
    EXPECT_EQ(source.read_count, 3);
}

// Red against Task 1: it calls end and setBoot and returns true.
TEST_F(FirmwareInstallerTest, VerifyFailureAbortsWithoutEndingOrSettingBoot) {
    verifier.verify_ok = false;

    EXPECT_FALSE(install());

    EXPECT_EQ(log,
              CallLog({"source.open",
                       "partition.begin",
                       "verifier.begin",
                       "source.close",
                       "verifier.verify",
                       "partition.abort"}));
}

// esp_ota_end releases the handle even when it fails, so aborting afterwards would be undefined.
// Red against Task 1: it calls setBoot and returns true.
TEST_F(FirmwareInstallerTest, EndFailureReturnsFalseWithoutAbort) {
    partition.end_ok = false;

    EXPECT_FALSE(install());

    EXPECT_EQ(log,
              CallLog({"source.open",
                       "partition.begin",
                       "verifier.begin",
                       "source.close",
                       "verifier.verify",
                       "partition.end"}));
}

// Red against Task 1: it returns true.
TEST_F(FirmwareInstallerTest, SetBootFailureReturnsFalseWithoutAbort) {
    partition.set_boot_ok = false;

    EXPECT_FALSE(install());

    EXPECT_EQ(log,
              CallLog({"source.open",
                       "partition.begin",
                       "verifier.begin",
                       "source.close",
                       "verifier.verify",
                       "partition.end",
                       "partition.setBoot"}));
}
```

- [ ] **Step 2: Run each test as it is added and confirm it fails for the stated reason**

Run: `./build.sh test-host`
Expected: only the newly added test fails, with a call-log mismatch or `install()` returning true. For the short-read test, see its comment: write the short-read check before the first run.

- [ ] **Step 3: The complete `install()`**

```cpp
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
```

Add `void abortDownload();  // aborts the partition, then closes the source` to the header's `private:` section.

- [ ] **Step 4: Run the whole suite**

Run: `./build.sh test-host`
Expected: 93 tests pass with no other output.

- [ ] **Step 5: Mutation check**

Delete or invert each guard in `install()` one at a time (the open check, begin check, short-read check, write check, verify check, end check, setBoot check, and the `abort()` inside `abortDownload`) and confirm at least one test fails each time. Restore the guard after each mutation. Note any guard that survives in the commit body.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i src/ota/FirmwareInstaller.h src/ota/FirmwareInstaller.cpp tests/unit/FirmwareInstallerTest.cpp
./build.sh test-host
git add src/ota/FirmwareInstaller.h src/ota/FirmwareInstaller.cpp tests/unit/FirmwareInstallerTest.cpp
git commit -m "feat(ota): abort FirmwareInstaller on download, flash and verify failures (#59)" -m "..."
```

---

### Task 3: Device implementations and the OtaInstall spike

These three classes are thin wrappers over hardware APIs and can't run on the host. Their test is the hardware spike in Task 4. TDD order for this task: write the spike first and watch it fail to compile because the classes are missing, then write the classes until it compiles.

**Files:**
- Create: `tests/spikes/OtaInstall/OtaInstall.ino`, `tests/spikes/OtaInstall/CMakeLists.txt`, `tests/spikes/OtaInstall/OtaInstallSettings.h.in`, `tests/spikes/OtaInstall/stall_server.py`, the `tests/spikes/OtaInstall/src/` symlinks
- Create: `src/ota/HttpFirmwareSource.h/.cpp`, `src/ota/EspOtaPartition.h/.cpp`, `src/ota/EcdsaImageVerifier.h/.cpp`
- Modify: `tests/spikes/CMakeLists.txt`

**Interfaces:**
- Consumes: the Task 1 interfaces; `OTA_PUBLIC_KEY_DER` from `src/security/OTAPublicKey.h`; `WeatherNetwork` (an `IHttpClient`) from `src/network/Network.h`; `OTAManager::checkForUpdate`
- Produces: `HttpFirmwareSource()`, `EspOtaPartition()`, `EcdsaImageVerifier()`, all default-constructible. D3 constructs them the same way the spike does.

- [ ] **Step 1: Spike build files**

`tests/spikes/OtaInstall/CMakeLists.txt`:
```cmake
# ABOUTME: Build and upload targets for the FirmwareInstaller hardware spike (D2 #59).
# ABOUTME: Not part of ALL; build with ./build.sh build-spike OtaInstall.
# Like OtaCheck, src/ here mirrors the real src/ with symlinks, because arduino-cli only compiles .cpp
# files inside the sketch directory.
set(OTA_INSTALL_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/OtaInstall")

# The stalled-server case needs stall_server.py (next to this file) running on the dev machine. Set
# OTA_STALL_URL in config.cmake to its URL, e.g. http://192.168.1.50:8000/firmware. Empty fails that case.
if(NOT DEFINED OTA_STALL_URL)
    set(OTA_STALL_URL "")
endif()
set(OTA_INSTALL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(OtaInstallSettings.h.in ${OTA_INSTALL_GENERATED_DIR}/OtaInstallSettings.h)

# min_spiffs matches the firmware's partition table, so flashing this spike doesn't swap layouts.
add_custom_target(OtaInstall
    COMMAND arduino-cli compile
        --fqbn ${INKPLATE10_FQBN}
        --board-options "${INKPLATE10_COMMON_BOARD_OPTIONS},PartitionScheme=min_spiffs"
        --build-property "compiler.cpp.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR} -I${OTA_INSTALL_GENERATED_DIR}"
        --build-property "compiler.c.extra_flags=-I${CMAKE_SOURCE_DIR}/src -I${GENERATED_INCLUDE_DIR} -I${OTA_INSTALL_GENERATED_DIR}"
        --build-path ${OTA_INSTALL_BUILD_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling OtaInstall spike via arduino-cli"
    VERBATIM
)

add_custom_target(upload-OtaInstall
    COMMAND sh -c "arduino-cli upload -p \"$SERIAL_PORT\" -b ${INKPLATE10_FQBN} --input-dir ${OTA_INSTALL_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS OtaInstall
    COMMENT "Uploading OtaInstall spike"
    VERBATIM
)
```

`tests/spikes/OtaInstall/OtaInstallSettings.h.in`:
```c
// ABOUTME: Generated settings for the OtaInstall spike, filled from config.cmake by configure_file.
// ABOUTME: OTA_STALL_URL points at stall_server.py on the dev machine, or is empty when unset.
#define OTA_STALL_URL "@OTA_STALL_URL@"
```

Append `add_subdirectory(OtaInstall)` to `tests/spikes/CMakeLists.txt`.

Symlink mirror. Run from the repo root; the targets are relative to each symlink's own directory, as in OtaCheck:
```bash
d=tests/spikes/OtaInstall/src; mkdir -p $d/network $d/ota $d/security
for f in IHttpClient.h Network.cpp Network.h NetworkLog.h; do ln -s ../../../../../src/network/$f $d/network/$f; done
for f in OTAManager.cpp OTAManager.h FirmwareInstaller.cpp FirmwareInstaller.h IFirmwareSource.h \
         IOtaPartition.h IImageVerifier.h HttpFirmwareSource.cpp HttpFirmwareSource.h EspOtaPartition.cpp \
         EspOtaPartition.h EcdsaImageVerifier.cpp EcdsaImageVerifier.h; do
    ln -s ../../../../../src/ota/$f $d/ota/$f; done
for f in CACerts.cpp CACerts.h OTAPublicKey.h; do ln -s ../../../../../src/security/$f $d/security/$f; done
```

- [ ] **Step 2: Stall server**

`tests/spikes/OtaInstall/stall_server.py`:
```python
#!/usr/bin/env python3
# ABOUTME: Throwaway HTTP server for the OtaInstall spike (#59): answers every GET, then stalls mid-body.
# ABOUTME: Proves HttpFirmwareSource's 30s read timeout turns a silent server into a failed install.
"""Stalling firmware server for the OtaInstall spike's timeout case.

Run it on the dev machine (not in the build container) before flashing the spike:

    python3 tests/spikes/OtaInstall/stall_server.py --port 8000

Then set OTA_STALL_URL in config.cmake to http://<this machine's LAN IP>:8000/firmware and re-run
./build.sh configure. Each GET gets 200 headers advertising --content-length bytes, --send-bytes of
body, then nothing for --hold-seconds before the connection closes. If the device can't connect, check
that the host firewall allows the port.
"""
import argparse
import http.server
import time


def make_handler(content_length, send_bytes, hold_seconds):
    class StallHandler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(content_length))
            self.end_headers()
            self.wfile.write(bytes(send_bytes))
            self.wfile.flush()
            client = self.client_address[0]
            print(f"{client}: sent {send_bytes} of {content_length} bytes, holding {hold_seconds}s", flush=True)
            time.sleep(hold_seconds)
            print(f"{client}: closing", flush=True)

    return StallHandler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8000, help="port to listen on (default: 8000)")
    parser.add_argument("--content-length", type=int, default=1758455, help="advertised body size")
    parser.add_argument(
        "--send-bytes",
        type=int,
        default=65536,
        help="body bytes sent before stalling; 64KB lines up with the installer's first progress report",
    )
    parser.add_argument("--hold-seconds", type=float, default=120, help="silence before closing")
    args = parser.parse_args()

    server = http.server.HTTPServer(
        ("0.0.0.0", args.port), make_handler(args.content_length, args.send_bytes, args.hold_seconds)
    )
    print(f"Stall server listening on port {args.port}; Ctrl-C to stop", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Spike sketch**

`tests/spikes/OtaInstall/OtaInstall.ino`:
```cpp
// ABOUTME: Throwaway D2 (#59) spike: drives the real FirmwareInstaller and device implementations through
// ABOUTME: tampered, stalled and dropped installs, then one valid install that boots from the other slot.
#include <ArduinoLog.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <inttypes.h>

#include "OtaInstallSettings.h"
#include "src/network/Network.h"
#include "src/ota/EcdsaImageVerifier.h"
#include "src/ota/EspOtaPartition.h"
#include "src/ota/FirmwareInstaller.h"
#include "src/ota/HttpFirmwareSource.h"
#include "src/ota/OTAManager.h"
#include "weather_station_2/user_settings.h"
#include "weather_station_2/version.h"

static const size_t DROP_AT_BYTES = 512 * 1024;
static const uint32_t WIFI_TIMEOUT_MS = 20000;
// HttpFirmwareSource's read timeout is 30s; the window allows for scheduling slop.
static const uint32_t STALL_MIN_MS = 29000;
static const uint32_t STALL_MAX_MS = 35000;

static HttpFirmwareSource source;
static EspOtaPartition partition;
static EcdsaImageVerifier verifier;
static FirmwareInstaller installer(source, partition, verifier);

static int failures = 0;
static uint32_t lastProgressMs = 0;
static bool dropArmed = false;

static void check(bool ok, const char* what) {
    Serial.printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const char* bootLabel() { return esp_ota_get_boot_partition()->label; }

static void printProgress(size_t received, size_t total) {
    lastProgressMs = millis();
    Serial.printf("  progress %u / %u\n", (unsigned)received, (unsigned)total);
}

static void dropWifiMidStream(size_t received, size_t total) {
    printProgress(received, total);
    if (dropArmed && received >= DROP_AT_BYTES) {
        dropArmed = false;
        Serial.println("  dropping WiFi");
        WiFi.disconnect();
    }
}

static bool reconnectWifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) return false;
        delay(250);
    }
    return true;
}

// Runs one install that must fail and checks that the boot partition did not change.
static void expectRejectedInstall(const char* name,
                                  const OTAManager::UpdateInfo& info,
                                  FirmwareInstaller::ProgressFn progress) {
    Serial.printf("\n--- %s ---\n", name);
    const char* before = bootLabel();
    uint32_t start = millis();
    bool installed = installer.install(info, progress);
    Serial.printf("%s: install=%d elapsed_ms=%lu boot before=%s after=%s\n",
                  name,
                  installed,
                  (unsigned long)(millis() - start),
                  before,
                  bootLabel());
    check(!installed && strcmp(before, bootLabel()) == 0, name);
}

void setup() {
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Serial.printf("\n=== OtaInstall spike, FIRMWARE_VERSION %" PRIu64 ", running %s, boot %s ===\n",
                  (uint64_t)FIRMWARE_VERSION,
                  esp_ota_get_running_partition()->label,
                  bootLabel());

    WeatherNetwork network(WIFI_SSID, WIFI_PASSWORD);
    network.begin();

    // currentVersion 0 accepts whatever is published, so the spike always gets a full UpdateInfo.
    OTAManager anyVersion(network, 0);
    OTAManager::UpdateInfo published;
    if (!anyVersion.checkForUpdate(OTA_MANIFEST_URL, published)) {
        check(false, "manifest fetched");
        Serial.printf("=== OtaInstall FAILED (%d failures) ===\n", failures);
        return;
    }
    Serial.printf("Published: version=%" PRIu64 " size=%u url=%s\n",
                  published.version,
                  (unsigned)published.size,
                  published.url);

    OTAManager::UpdateInfo info = published;
    info.sha256[0] = info.sha256[0] == '0' ? '1' : '0';
    expectRejectedInstall("tampered sha256", info, printProgress);

    info = published;
    // Character 20 decodes into the DER r integer, so the signature still parses and the ECDSA math rejects it.
    info.signature[20] = info.signature[20] == 'A' ? 'B' : 'A';
    expectRejectedInstall("tampered signature", info, printProgress);

    static_assert(sizeof(OTA_STALL_URL) <= sizeof(info.url), "OTA_STALL_URL does not fit UpdateInfo::url");
    if (strlen(OTA_STALL_URL) == 0) {
        check(false, "stalled server (OTA_STALL_URL not set in config.cmake)");
    } else {
        info = published;
        strcpy(info.url, OTA_STALL_URL);
        expectRejectedInstall("stalled server", info, printProgress);
        uint32_t stallMs = millis() - lastProgressMs;
        Serial.printf("stalled server: gave up %lu ms after the last progress report\n", (unsigned long)stallMs);
        check(stallMs >= STALL_MIN_MS && stallMs <= STALL_MAX_MS, "stall ended by the 30s read timeout");
    }

    info = published;
    dropArmed = true;
    expectRejectedInstall("mid-stream WiFi drop", info, dropWifiMidStream);
    check(reconnectWifi(), "WiFi reconnected after drop");

    Serial.println("\n--- valid install ---");
    const char* running = esp_ota_get_running_partition()->label;
    uint32_t start = millis();
    bool installed = installer.install(published, printProgress);
    Serial.printf("valid install: install=%d elapsed_ms=%lu running=%s boot=%s\n",
                  installed,
                  (unsigned long)(millis() - start),
                  running,
                  bootLabel());
    check(installed && strcmp(running, bootLabel()) != 0, "valid install switched the boot partition");

    Serial.printf("=== OtaInstall %s (%d failures) ===\n", failures == 0 ? "PASSED" : "FAILED", failures);
    if (installed) {
        Serial.println("Restarting into the new image");
        Serial.flush();
        esp_restart();
    }
}

void loop() {}
```

- [ ] **Step 4: Watch the spike fail to compile**

```bash
./build.sh configure && ./build.sh build-spike OtaInstall
```
Expected: the compile fails on the missing `HttpFirmwareSource.h`, `EspOtaPartition.h` and `EcdsaImageVerifier.h` (the symlinks dangle).

- [ ] **Step 5: `HttpFirmwareSource`**

`src/ota/HttpFirmwareSource.h`:
```cpp
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
    WiFiClient* m_stream = nullptr;
};

#endif  // HTTP_FIRMWARE_SOURCE_H
```

`src/ota/HttpFirmwareSource.cpp`:
```cpp
// ABOUTME: Streams a firmware image over plain HTTP from the local json-store with a 30s read timeout.
// ABOUTME: Device-only implementation of IFirmwareSource; a stalled or dropped connection ends as a short read.
#include "HttpFirmwareSource.h"

#include "../network/NetworkLog.h"

// Long enough to ride out WiFi hiccups, short enough that a dead server can't keep the radio on for long.
static const uint16_t READ_TIMEOUT_MS = 30000;

bool HttpFirmwareSource::open(const char* url) {
    m_http.setTimeout(READ_TIMEOUT_MS);
    if (!m_http.begin(url)) {
        NET_LOG_WARNING("OTA firmware URL rejected: %s", url);
        return false;
    }
    int code = m_http.GET();
    if (code != HTTP_CODE_OK) {
        NET_LOG_WARNING("OTA firmware GET failed: %d", code);
        m_http.end();
        return false;
    }
    m_stream = m_http.getStreamPtr();
    return true;
}

size_t HttpFirmwareSource::read(uint8_t* buf, size_t len) {
    if (m_stream == nullptr) return 0;
    // readBytes blocks until len bytes arrive or the stream timeout expires.
    return m_stream->readBytes(buf, len);
}

void HttpFirmwareSource::close() {
    m_http.end();
    m_stream = nullptr;
}
```

- [ ] **Step 6: `EspOtaPartition`**

`src/ota/EspOtaPartition.h`:
```cpp
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
```

`src/ota/EspOtaPartition.cpp`:
```cpp
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
```

- [ ] **Step 7: `EcdsaImageVerifier`**

`src/ota/EcdsaImageVerifier.h`:
```cpp
// ABOUTME: Hashes a streamed firmware image with SHA-256 and checks it against the signed manifest.
// ABOUTME: Device-only implementation of IImageVerifier; ECDSA P-256 over the digest with OTA_PUBLIC_KEY_DER.
#ifndef ECDSA_IMAGE_VERIFIER_H
#define ECDSA_IMAGE_VERIFIER_H

#include <mbedtls/sha256.h>

#include "IImageVerifier.h"

class EcdsaImageVerifier : public IImageVerifier {
   public:
    EcdsaImageVerifier();
    ~EcdsaImageVerifier() override;
    void begin() override;
    void update(const uint8_t* buf, size_t len) override;
    bool verify(const char* sha256Hex, const char* signatureB64) override;

   private:
    mbedtls_sha256_context m_sha;
};

#endif  // ECDSA_IMAGE_VERIFIER_H
```

`src/ota/EcdsaImageVerifier.cpp`:
```cpp
// ABOUTME: Hashes a streamed firmware image with SHA-256 and checks it against the signed manifest.
// ABOUTME: Device-only implementation of IImageVerifier; ECDSA P-256 over the digest with OTA_PUBLIC_KEY_DER.
#include "EcdsaImageVerifier.h"

#include <mbedtls/base64.h>
#include <mbedtls/pk.h>

#include <cstdlib>
#include <cstring>

#include "../security/OTAPublicKey.h"
#include "../network/NetworkLog.h"

static const size_t DIGEST_SIZE = 32;
static const size_t MAX_SIGNATURE_SIZE = 80;  // DER ECDSA P-256 signatures are at most 72 bytes

static bool fromHex(const char* hex, uint8_t* bytes, size_t len) {
    if (strlen(hex) != len * 2) return false;
    for (size_t i = 0; i < len; ++i) {
        char pair[3] = {hex[2 * i], hex[2 * i + 1], '\0'};
        char* end;
        bytes[i] = (uint8_t)strtoul(pair, &end, 16);
        if (*end != '\0') return false;
    }
    return true;
}

EcdsaImageVerifier::EcdsaImageVerifier() { mbedtls_sha256_init(&m_sha); }

EcdsaImageVerifier::~EcdsaImageVerifier() { mbedtls_sha256_free(&m_sha); }

void EcdsaImageVerifier::begin() {
    // An aborted install never reaches verify(), so release any previous context here. On the ESP32 a
    // hashing context can hold the hardware SHA engine until it is freed.
    mbedtls_sha256_free(&m_sha);
    mbedtls_sha256_init(&m_sha);
    mbedtls_sha256_starts(&m_sha, 0);  // 0 = SHA-256, not SHA-224
}

void EcdsaImageVerifier::update(const uint8_t* buf, size_t len) { mbedtls_sha256_update(&m_sha, buf, len); }

bool EcdsaImageVerifier::verify(const char* sha256Hex, const char* signatureB64) {
    uint8_t digest[DIGEST_SIZE];
    mbedtls_sha256_finish(&m_sha, digest);
    mbedtls_sha256_free(&m_sha);
    mbedtls_sha256_init(&m_sha);

    uint8_t expected[DIGEST_SIZE];
    if (!fromHex(sha256Hex, expected, DIGEST_SIZE)) {
        NET_LOG_WARNING("OTA manifest sha256 is not valid hex");
        return false;
    }
    if (memcmp(digest, expected, DIGEST_SIZE) != 0) {
        NET_LOG_WARNING("OTA image SHA-256 does not match the manifest");
        return false;
    }

    uint8_t signature[MAX_SIGNATURE_SIZE];
    size_t signatureLen = 0;
    int ret = mbedtls_base64_decode(
        signature, sizeof(signature), &signatureLen, (const uint8_t*)signatureB64, strlen(signatureB64));
    if (ret != 0) {
        NET_LOG_WARNING("OTA signature is not valid base64: %d", ret);
        return false;
    }

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    ret = mbedtls_pk_parse_public_key(&key, OTA_PUBLIC_KEY_DER, sizeof(OTA_PUBLIC_KEY_DER));
    if (ret == 0) ret = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, DIGEST_SIZE, signature, signatureLen);
    mbedtls_pk_free(&key);
    if (ret != 0) {
        NET_LOG_WARNING("OTA signature rejected: %d", ret);
        return false;
    }
    return true;
}
```

- [ ] **Step 8: Build the spike, the firmware and the host tests**

```bash
./build.sh build-spike OtaInstall
./build.sh build                 # firmware compiles the new src/ota files; size check must still pass
./build.sh build-spike OtaCheck  # still compiles with the repaired mirror
./build.sh test-host             # 93 pass, nothing from the device-only files leaks into the host build
```
Expected: all four succeed. Record the firmware size line (it was 1758455 B, 89%). Any compiler warning in the new files gets fixed, not ignored.

- [ ] **Step 9: Format and commit**

```bash
clang-format -i src/ota/HttpFirmwareSource.* src/ota/EspOtaPartition.* src/ota/EcdsaImageVerifier.* \
    tests/spikes/OtaInstall/OtaInstall.ino
git add src/ota/HttpFirmwareSource.h src/ota/HttpFirmwareSource.cpp src/ota/EspOtaPartition.h \
    src/ota/EspOtaPartition.cpp src/ota/EcdsaImageVerifier.h src/ota/EcdsaImageVerifier.cpp \
    tests/spikes/OtaInstall tests/spikes/CMakeLists.txt
git status   # confirm only these paths are staged; no .worktrees/, no build output
git commit -m "feat(ota): add device FirmwareInstaller implementations and OtaInstall spike (#59)" -m "..."
```

---

### Task 4: Run the spike on hardware

**Files:** none committed, except a fix if the spike finds a bug. A fix starts with a failing host test where the bug is in sequencing logic.

- [ ] **Step 1: Start the stall server and point the spike at it**

```bash
ip -4 -o addr show | awk '{print $2, $4}'   # pick the LAN address on the same subnet as 192.168.1.x
```
Run `python3 tests/spikes/OtaInstall/stall_server.py --port 8000` in the background (Bash `run_in_background`). Add `set(OTA_STALL_URL "http://<LAN IP>:8000/firmware")` to `config.cmake` (gitignored). Then run `./build.sh configure` and check that `build/tests/spikes/OtaInstall/generated/OtaInstallSettings.h` contains the URL.

- [ ] **Step 2: Publish a real build to the live manifest**

```bash
./build.sh build
python3 tools/ota_publish.py --firmware build/WeatherStation/Weather_Station_2.ino.bin \
    --private-key keys/ecdsa_private.pem --pi-host 192.168.1.2:5000
curl -s http://192.168.1.2:5000/ota-firmware/data
```
The version stays 20260728001, and nothing polls the manifest yet, so overwriting it is safe. The manifest `size` must equal the size of the `.bin` file.

- [ ] **Step 3: Flash the spike and capture**

```bash
./build.sh flash-spike OtaInstall
python3 tools/serial_capture.py --seconds 300
```
Each case streams about 1.75 MB after a full-slot erase, and the stall case adds 30 s. If the capture ends before `=== OtaInstall`, rerun with more seconds.

Expected:
- `PASS` for tampered sha256 (`SHA-256 does not match`), tampered signature (`signature rejected`), stalled server (its stall time is within 29–35 s), mid-stream drop, and WiFi reconnect. Each rejected case shows `boot before=app0 after=app0`.
- `valid install: install=1 ... running=app0 boot=app1`, then `=== OtaInstall PASSED (0 failures) ===`, then a restart into the normal weather station boot (`Firmware version: <GIT_VERSION>`, `[HTTPS] GET... code: 200`). Since app0 holds the spike, a weather station boot can only come from app1. If the bootloader logs `Loaded app from partition at offset 0x1f0000`, note it.
- `Erased OTA partition app1 in N ms` on every case. Record N, plus each case's elapsed_ms for the valid install, and compute stream time as elapsed minus erase minus about 0.5 s for verify.
- No task watchdog, panic or `Guru Meditation` lines. A watchdog during the full-slot erase is a real finding: stop and raise it with Jesse.

If the stall case reports far more or less than 30 s, HTTPClient's timeout isn't reaching `readBytes`. Stop, find the root cause in the arduino-esp32 `HTTPClient` / `NetworkClient` sources, fix `HttpFirmwareSource`, and rerun.

- [ ] **Step 4: Restore the real firmware on app0**

```bash
./build.sh flash
python3 tools/serial_capture.py --seconds 40
```
Expected: normal boot output (see `CLAUDE.md`). The USB upload rewrites otadata, so the device boots app0. Stop the stall server, and remove the `OTA_STALL_URL` line from `config.cmake` (or leave it, since it's gitignored, and tell Jesse).

---

### Task 5: Roadmap, memory and PR

**Files:**
- Modify: `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md`

- [ ] **Step 1: Update the roadmap**
- Story index row D2: 🔄 → ✅.
- In the D2 design, replace "The 4KB chunk buffer is a `FirmwareInstaller` member, not a stack array, because the Arduino main task has an 8KB stack." with "The 4KB chunk buffer is a file-scope static in `FirmwareInstaller.cpp`: as a member it would still land on the 8KB loop-task stack whenever the installer is a local."
- Under D2 Verification, add the measured results: erase ms, stream ms, the valid install's total ms, the stall-timeout ms, and the firmware size after Task 3.

- [ ] **Step 2: Commit, push, PR**

```bash
git add docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md docs/superpowers/plans/2026-11-08-ota-d2-firmware-installer.md
git commit -m "docs: mark D2 done and record OtaInstall spike timings (#59)" -m "..."
git push -u origin feat/issue-59-firmware-installer
gh pr create --title "feat(ota): FirmwareInstaller::install (D2, #59)" --body "..."
```
The PR body says "Closes #59" and covers the design summary, the test list, the spike results with timings, and the buffer deviation. It ends with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`. If the SSH push hangs, push over HTTPS (see memory `reference_git_push_https.md`).

- [ ] **Step 3: Update memory**

In `project_ota_design.md`: D2 done in PR <n>, next D3 #60, and the measured erase and stream times.

## Verification summary

- Host: `./build.sh test-host` gives 93 tests passing with clean output. The mutation check in Task 2 shows each guard is covered.
- Build: firmware, OtaCheck and OtaInstall all compile, and the firmware stays under the size budget.
- Hardware: the OtaInstall spike passes every case, the device boots the published firmware from app1, and the USB reflash restores app0.
