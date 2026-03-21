// Network.h - Improved error handling
#ifndef NETWORK_H
#define NETWORK_H

#include <Stream.h>
#include <StreamString.h>
#include <WString.h>
#include <WiFiClient.h>

#include <memory>

// Network error codes
const int NETWORK_OK = 0;
const int NETWORK_WIFI_ERROR = -1;
const int NETWORK_HTTP_ERROR = -2;
const int NETWORK_TIMEOUT_ERROR = -3;
const int NETWORK_NO_DATA = -4;

class Network {
   public:
    Network(const char* ssid, const char* password);
    void begin();
    void syncTime();

    // Modified to return error code and allow retry configuration
    int get(WiFiClient& client, const String& url, StreamString& stream, int retries = 2, int timeout = 10000);

    // Get the text representation of an error code
    const char* getErrorString(int errorCode);

   private:
    bool reconnect(int maxAttempts = 7);
    char m_ssid[32];
    char m_password[32];
};
#endif  // NETWORK_H
