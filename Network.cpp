#include "Network.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoLog.h>

Network::Network(const char* ssid, const char* password) {
  strncpy(m_ssid, ssid, sizeof(m_ssid));
  m_ssid[sizeof(m_ssid) - 1] = '\0';

  strncpy(m_password, password, sizeof(m_password));
  m_password[sizeof(m_password) - 1] = '\0';
}

void Network::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(m_ssid, m_password);

  int cnt = 0;
  Serial.print(F("Waiting for WiFi to connect"));
  while ((WiFi.status() != WL_CONNECTED)) {
    Serial.print(F("."));
    delay(1000);
    ++cnt;

    if (cnt == 20) {
      Log.errorln("Can't connect to WIFI, restarting");
      delay(100);
      ESP.restart();
    }
  }
  Serial.println(F(" connected"));

  this->syncTime();
}

void Network::syncTime() {
  // Used for setting correct time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    // Print a dot every half a second while time is not set
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  // Used to store time info
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);

  Log.notice(F("Current time: %s" CR), asctime(&timeinfo));
}

void Network::get(WiFiClient& client, const String& url, StreamString& stream) {
  HTTPClient https;

  this->reconnect();

  Log.notice(F("[HTTPS] begin..." CR));

  if (https.begin(client, url.c_str())) {  // HTTPS
    Log.notice(F("[HTTPS] GET..." CR));
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Log.notice(F("[HTTPS] GET... code: %d" CR), httpCode);

      if (httpCode == HTTP_CODE_OK)
      {
	  static_cast<String&>(stream) = https.getString();
      }

    } else {
      Log.error(F("[HTTPS] GET... failed, error: %s" CR), https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Log.error(F("[HTTPS] Unable to connect" CR));
  }
}

bool Network::reconnect() {
  // If not connected to wifi reconnect wifi
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();

    delay(5000);

    int cnt = 0;
    Log.notice(F("Waiting for WiFi to reconnect..."));
    while ((WiFi.status() != WL_CONNECTED)) {
      // Prints a dot every second that wifi isn't connected
      Serial.print(F("."));
      delay(1000);
      ++cnt;

      if (cnt == 7) {
        Log.error(F("Can't connect to WIFI, restart initiated." CR));
        return false;
      }
    }
  }

  return true;
}
