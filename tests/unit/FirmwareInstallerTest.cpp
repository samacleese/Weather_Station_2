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
        strcpy(info.signature,
               "MEUCICfe4BxnC12bksqLrOKRyKaOXFDcnTD2bALc/xdm+o7xAiEAiGdK+EOHgeBJc8ms4a5gVcG3VXvPoZsNL4N8fVvo/FU=");
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
