// ABOUTME: Interface for HTTP GET requests, abstracting WiFi for testability.
// ABOUTME: Implement with Network for production; stub for tests.
#ifndef IHTTPCLIENT_H
#define IHTTPCLIENT_H

#include <string>

class IHttpClient {
   public:
    virtual ~IHttpClient() = default;
    // Performs an HTTPS GET. Returns 0 on success or a negative error code.
    // On success, body contains the response body.
    virtual int get(const std::string& url, std::string& body) = 0;
};

#endif  // IHTTPCLIENT_H
