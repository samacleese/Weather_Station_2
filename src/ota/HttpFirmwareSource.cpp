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
