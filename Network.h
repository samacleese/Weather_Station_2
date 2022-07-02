#ifndef NETWORK_H
#define NETWORK_H

#include <WString.h>
#include <Stream.h>
#include <StreamString.h>
#include <WiFiClient.h>

#include <memory>

class Network {
 public:
  Network(const char* ssid, const char* password);
  void begin();
  void syncTime();
  void get(WiFiClient& client, const String& url, StreamString& stream);

 private:
  bool reconnect();
  char m_ssid[32];
  char m_password[32];
};
#endif  // NETWORK_H
