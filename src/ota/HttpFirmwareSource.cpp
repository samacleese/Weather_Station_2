// ABOUTME: Streams a firmware image over plain HTTP from the local json-store, bounded by a 30s inactivity
// ABOUTME: timeout and a whole-download deadline. Device-only IFirmwareSource; a stall ends as a short read.
#include "HttpFirmwareSource.h"

#include "../network/NetworkLog.h"

// Inactivity timeout: long enough to ride out WiFi hiccups, short enough that a dead server can't keep the
// radio on for long. It is the only bound on GET(): HTTPClient restarts it on every header byte, so a server
// that trickles headers can hold GET() open past the download deadline, which covers the body only.
// WeatherNetwork::get has the same exposure; both talk to trusted servers. Tracked in issue #82.
static const uint16_t READ_TIMEOUT_MS = 30000;

bool HttpFirmwareSource::open(const char* url) {
    m_openedMs = millis();
    m_http.setTimeout(READ_TIMEOUT_MS);
    // One-shot multi-MB download: reuse would carry a half-read body or the wrong host into the next open.
    m_http.setReuse(false);
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
    // NetworkClient::readBytes restarts its timeout whenever any byte arrives, so a server that trickles
    // data could hold one read open for hours. This loop also enforces the whole-download deadline.
    size_t got = 0;
    uint32_t lastDataMs = millis();
    while (got < len) {
        uint32_t now = millis();
        if (now - m_openedMs >= m_deadlineMs) {
            NET_LOG_WARNING("OTA download passed its %d ms deadline", (int)m_deadlineMs);
            break;
        }
        int n = m_stream->read(buf + got, len - got);  // non-blocking: 0 means no data yet
        if (n < 0) break;
        if (n > 0) {
            got += n;
            lastDataMs = now;
            continue;
        }
        if (now - lastDataMs >= READ_TIMEOUT_MS) {
            NET_LOG_WARNING("OTA download received nothing for %d ms", (int)READ_TIMEOUT_MS);
            break;
        }
        delay(2);  // let the WiFi task run
    }
    return got;
}

void HttpFirmwareSource::close() {
    m_http.end();
    m_stream = nullptr;
}
