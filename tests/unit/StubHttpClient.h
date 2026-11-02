// ABOUTME: Hand-written IHttpClient test double shared by host unit tests.
// ABOUTME: Returns a canned body and result code, and records the URL it was asked for.
#ifndef STUB_HTTP_CLIENT_H
#define STUB_HTTP_CLIENT_H

#include <string>

#include "network/IHttpClient.h"

class StubHttpClient : public IHttpClient {
   public:
    int next_result = 0;
    std::string next_body;
    int call_count = 0;
    std::string last_url;

    int get(const std::string& url, std::string& body) override {
        call_count++;
        last_url = url;
        body = next_body;
        return next_result;
    }
};

#endif  // STUB_HTTP_CLIENT_H
