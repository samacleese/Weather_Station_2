// ABOUTME: Classifies IHttpClient::get() result codes into failure categories.
// ABOUTME: Shared by the weather.gov clients so each maps failures onto its own error codes.
#ifndef NETWORK_RESULT_H
#define NETWORK_RESULT_H

enum class NetworkFailure { None, Connection, Certificate, Other };

// The values mirror the NetworkError enum in Network.h. That header isn't included here,
// so host tests compile without WiFi headers. Keep the two in sync.
inline NetworkFailure classifyNetworkResult(int result) {
    switch (result) {
        case 0:
            return NetworkFailure::None;
        case -1:  // NETWORK_WIFI_ERROR
        case -2:  // NETWORK_HTTP_ERROR
        case -3:  // NETWORK_TIMEOUT_ERROR
        case -4:  // NETWORK_NO_DATA
            return NetworkFailure::Connection;
        case -5:  // NETWORK_CERT_ERROR
            return NetworkFailure::Certificate;
        default:
            return NetworkFailure::Other;
    }
}

#endif  // NETWORK_RESULT_H
