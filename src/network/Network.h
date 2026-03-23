// ABOUTME: WiFi connection management and HTTPS request handling for ESP32.
// ABOUTME: Provides error codes and retry logic for network operations.
#ifndef NETWORK_H
#define NETWORK_H

#include <Stream.h>
#include <StreamString.h>
#include <WString.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <memory>

enum NetworkError {
    NETWORK_OK = 0,
    NETWORK_WIFI_ERROR = -1,
    NETWORK_HTTP_ERROR = -2,
    NETWORK_TIMEOUT_ERROR = -3,
    NETWORK_NO_DATA = -4,
    NETWORK_CERT_ERROR = -5,
};

class Network {
   public:
    Network(const char* ssid, const char* password);
    void begin();
    void syncTime();

    // Modified to return error code and allow retry configuration
    int get(WiFiClientSecure& client, const String& url, StreamString& stream, int retries = 2, int timeout = 10000);

    // Get the text representation of an error code
    const char* getErrorString(int errorCode);

   private:
    bool reconnect(int maxAttempts = 7);
    bool isCertError(int httpCode, WiFiClientSecure& client);
    char m_ssid[32];
    char m_password[32];
};
#endif  // NETWORK_H
