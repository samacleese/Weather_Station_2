// ABOUTME: Throwaway spike C4 (#57): polls the GPIO 36 wake button every 100ms while awake, with the display
// ABOUTME: initialised, WiFi connected and the battery ADC in use, to check it reads cleanly outside deep sleep.
#include <Inkplate.h>
#include <WiFi.h>

#include "weather_station_2/user_settings.h"

constexpr uint8_t WAKE_BUTTON_PIN = 36;
constexpr uint32_t POLL_INTERVAL_MS = 100;
constexpr uint32_t BATTERY_READ_INTERVAL_MS = 1000;
constexpr uint32_t IDLE_PHASE_MS = 10000;
constexpr uint32_t PRESS_PHASE_MS = 20000;
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;

Inkplate display(INKPLATE_1BIT);

struct PhaseResult {
    uint32_t samples = 0;
    uint32_t lowSamples = 0;
    uint32_t presses = 0;  // HIGH -> LOW transitions
};

static void showMessage(const char* message) {
    display.clearDisplay();
    display.setTextSize(4);
    display.setCursor(50, 300);
    display.print(message);
    display.display();
}

// Polls the button for `durationMs`, logging every level change; reads the battery once a second
// so the SAR ADC powers up during polling, as it will in the real firmware.
static PhaseResult pollPhase(const char* name, uint32_t durationMs) {
    PhaseResult result;
    int previous = HIGH;
    uint32_t start = millis();
    uint32_t lastBatteryRead = 0;
    while (millis() - start < durationMs) {
        uint32_t now = millis() - start;
        if (now - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
            display.readBattery();
            lastBatteryRead = now;
        }
        int level = digitalRead(WAKE_BUTTON_PIN);
        ++result.samples;
        if (level == LOW) ++result.lowSamples;
        if (level != previous) {
            Serial.printf("phase=%s t_ms=%lu level=%s\n", name, (unsigned long)now, level == LOW ? "LOW" : "HIGH");
            if (level == LOW) ++result.presses;
            previous = level;
        }
        delay(POLL_INTERVAL_MS);
    }
    Serial.printf("phase=%s samples=%u low_samples=%u presses=%u\n",
                  name,
                  (unsigned)result.samples,
                  (unsigned)result.lowSamples,
                  (unsigned)result.presses);
    return result;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("SPIKE-BEGIN WakeButton");

    display.begin();
    pinMode(WAKE_BUTTON_PIN, INPUT);  // input-only pin with an external pull-up on the Inkplate 10

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS) delay(250);
    Serial.printf("WiFi %s, battery %.3f V\n",
                  WiFi.status() == WL_CONNECTED ? "connected" : "NOT connected",
                  display.readBattery());

    showMessage("Don't touch the button");
    Serial.println("PROMPT idle: don't touch the button");
    PhaseResult idle = pollPhase("idle", IDLE_PHASE_MS);

    showMessage("Press the button now");
    Serial.println("PROMPT press: press the button and count the presses");
    PhaseResult press = pollPhase("press", PRESS_PHASE_MS);

    showMessage("Done");
    Serial.printf("SUMMARY idle_low_samples=%u press_count=%u\n", (unsigned)idle.lowSamples, (unsigned)press.presses);
    Serial.println("SPIKE-END");
}

void loop() {}
