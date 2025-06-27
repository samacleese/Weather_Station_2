// Network.cpp - Improved error handling
#include "Network.h"

#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <WiFi.h>

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

int Network::get(WiFiClient& client, const String& url, StreamString& stream, int retries, int timeout) {
    HTTPClient https;
    int attempt = 0;
    int httpCode = 0;

    while (attempt <= retries) {
        if (attempt > 0) {
            Log.notice(F("[HTTPS] Retry attempt %d of %d" CR), attempt, retries);
            delay(2000 * attempt);  // Increasing backoff delay between retries
        }

        if (!this->reconnect()) {
            Log.error(F("[HTTPS] WiFi reconnection failed" CR));
            return NETWORK_WIFI_ERROR;
        }

        Log.notice(F("[HTTPS] begin..." CR));

        https.setTimeout(timeout);  // Set timeout for the HTTP request

        if (https.begin(client, url.c_str())) {  // HTTPS
            Log.notice(F("[HTTPS] GET..." CR));
            // start connection and send HTTP header
            httpCode = https.GET();

            // httpCode will be negative on error
            if (httpCode > 0) {
                // HTTP header has been sent and Server response header has been handled
                Log.notice(F("[HTTPS] GET... code: %d" CR), httpCode);

                if (httpCode == HTTP_CODE_OK) {
                    String payload = https.getString();
                    if (payload.length() > 0) {
                        static_cast<String&>(stream) = payload;
                        https.end();
                        return NETWORK_OK;
                    } else {
                        Log.warning(F("[HTTPS] Received empty response" CR));
                        https.end();
                        attempt++;
                        continue;  // Try again
                    }
                } else {
                    Log.warning(F("[HTTPS] HTTP error code: %d" CR), httpCode);
                    https.end();
                    attempt++;
                    continue;  // Try again
                }
            } else {
                Log.error(F("[HTTPS] GET... failed, error: %s" CR), https.errorToString(httpCode).c_str());
                https.end();
                attempt++;
                continue;  // Try again
            }
        } else {
            Log.error(F("[HTTPS] Unable to connect" CR));
            attempt++;
            continue;  // Try again
        }
    }

    // If we got here, all retries failed
    if (httpCode > 0) {
        return NETWORK_HTTP_ERROR;
    } else if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED) {
        return NETWORK_WIFI_ERROR;
    } else if (httpCode == HTTPC_ERROR_READ_TIMEOUT) {
        return NETWORK_TIMEOUT_ERROR;
    } else {
        return NETWORK_NO_DATA;
    }
}

bool Network::reconnect(int maxAttempts) {
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

            if (cnt == maxAttempts) {
                Log.error(F("Can't connect to WIFI, restart initiated." CR));
                return false;
            }
        }
        Log.notice(F(" connected" CR));
    }

    return true;
}

const char* Network::getErrorString(int errorCode) {
    switch (errorCode) {
        case NETWORK_OK:
            return "No error";
        case NETWORK_WIFI_ERROR:
            return "WiFi connection error";
        case NETWORK_HTTP_ERROR:
            return "HTTP request error";
        case NETWORK_TIMEOUT_ERROR:
            return "Request timeout";
        case NETWORK_NO_DATA:
            return "No data received";
        default:
            return "Unknown error";
    }
}
