// ABOUTME: SSL CA certificate store mapping hostnames to PEM-encoded CA certs.
// ABOUTME: Used by Network to configure TLS before HTTPS requests.
#ifndef CACERTS_H
#define CACERTS_H

#include <map>
#include <string>

class CACerts {
   public:
    static const char* getCert(const std::string& host);

   private:
    static const std::map<std::string, const char*> ca_certs;
};

#endif  // CACERTS_H
