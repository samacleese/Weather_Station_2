#include <ArduinoLog.h>
#include <Inkplate.h>
#include <rom/rtc.h>

#include <memory>

#include "driver/rtc_io.h"

// TODO - move this to a seperate header file to add make_unique
namespace std {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace std

#ifndef ARDUINO_INKPLATE10
#error "Wrong board selection for this example, please select Inkplate 10 in the boards menu."
#endif

#include "CACerts.h"
#include "CurrentConditions.h"
#include "DisplayLocations.h"
#include "Fonts/Roboto_Light.h"
#include "Fonts/Roboto_Medium.h"
#include "Kitties.h"
#include "KittyPics.h"
#include "Network.h"

#define US_PER_SEC 1000000ull
#define TIME_TO_SLEEP_S 600
#define ERROR_TIME_TO_SLEEP_S 300

Inkplate display(INKPLATE_3BIT);

char buffer[256];

void setup() {
    float ADC_OFFSET = -0.50;
    auto network = std::make_shared<Network>("NorwegianFish", "rufalina");
    rtc_get_reset_reason(0);
    DisplayLocation kitties(850, 50, 300, 300);
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Log.setShowLevel(true);

    display.begin();

    // Initialize display with loading message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(0, 7);
    display.setFont(Roboto_Medium.at(35));
    display.setCursor(300, 350);
    display.print("Weather Station Loading...");
    display.display();

    network->begin();

    CurrentConditions curr(network);

    const uint8_t* nextKitty = Kitties::getNextKitty();

    // Update weather with retry capability
    int updateResult = curr.update(3);  // Try up to 3 times

    display.clearDisplay();

    // Handle error cases with user feedback
    if (updateResult != CURRENT_CONDITIONS_OK) {
        // Show error message on display
        display.setTextSize(1);
        display.setTextColor(0, 7);
        display.setCursor(100, 370);
        sprintf(buffer, "Weather data error: %s", curr.getErrorString(updateResult));
        display.print(buffer);

        // If we have partial data, still show it
        if (curr.temperature != -999 || curr.wind_speed != -999) {
            display.setCursor(100, 405);
            display.print("Showing partial data");
        } else {
            display.setCursor(100, 405);
            display.print("No weather data available");

            // Show time of attempt and error code for debugging
            display.setCursor(100, 440);
            time_t now;
            time(&now);
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            sprintf(buffer,
                    "Last attempt: %02d:%02d:%02d UTC - Error code: %d",
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec,
                    updateResult);
            display.print(buffer);

            // Draw kitty for user comfort
            display.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);

            // Get and show battery info at least
            int temperature = display.readTemperature();
            float voltage = display.readBattery() + ADC_OFFSET;

            display.setFont(Roboto_Light.at(42));
            display.setCursor(860, 400);
            display.print(voltage, 2);
            display.print('V');
            display.print(' ');
            display.print(temperature, DEC);
            display.print('C');

            display.display();

            // Go to sleep and try again later
            rtc_gpio_isolate(GPIO_NUM_12);
            esp_sleep_enable_ext1_wakeup((1ULL << 36), ESP_EXT1_WAKEUP_ALL_LOW);
            esp_sleep_enable_timer_wakeup(ERROR_TIME_TO_SLEEP_S * US_PER_SEC);
            esp_deep_sleep_start();
            return;  // Exit early
        }
    }

    // Draw Temperature with validity check
    display.setTextSize(1);
    display.setTextColor(0, 7);
    display.setFont(Roboto_Medium.at(150));
    display.setCursor(50, 150);
    if (curr.temperature != -999) {
        sprintf(buffer, "%d C", curr.temperature);
    } else {
        sprintf(buffer, "-- C");
    }
    display.print(buffer);

    // Draw Windspeed with validity check
    display.setCursor(50, 325);
    if (curr.wind_speed != -999) {
        sprintf(buffer, "%d km/h", curr.wind_speed);
    } else {
        sprintf(buffer, "-- km/h");
    }
    display.print(buffer);

    // Raw Message
    display.setFont(Roboto_Medium.at(20));
    display.setCursor(20, 750);
    sprintf(buffer, "%s", curr.raw_message);
    display.print(buffer);

    // Weather description
    display.setFont(Roboto_Light.at(150));
    display.setCursor(50, 620);
    sprintf(buffer, "%s", curr.description);
    {
        int16_t x, y, x1, y1;
        uint16_t w, h;
        x = 50;
        y = 150;
        display.getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
        Log.info(F("Min X = %d, Min Y = %d, Max X = %ud, Max y = %ud" CR), x1, y1, w, h);
        if (w > 1100) {
            Log.info(F("Reducing description font!" CR));
            display.setFont(Roboto_Light.at(104));
        }
    }
    display.print(buffer);

    // Draw kitty
    display.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);

    // Get the temp and battery voltage
    int temperature;
    float voltage;

    temperature = display.readTemperature();
    voltage = display.readBattery() + ADC_OFFSET;

    // Display system info
    display.setFont(Roboto_Light.at(42));
    display.setCursor(860, 400);
    display.print(voltage, 2);
    display.print('V');
    display.print(' ');
    display.print(temperature, DEC);
    display.print('C');

    // If there was a warning but we have data, show it
    if (updateResult != CURRENT_CONDITIONS_OK) {
        display.setFont(Roboto_Light.at(24));
        display.setCursor(860, 450);
        display.print("Warning: ");
        display.print(curr.getErrorString(updateResult));
    }

    display.display();

    // Goto deep sleep
    rtc_gpio_isolate(GPIO_NUM_12);
    esp_sleep_enable_ext1_wakeup((1ULL << 36), ESP_EXT1_WAKEUP_ALL_LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_S * US_PER_SEC);
    esp_deep_sleep_start();
}

void loop() {
    // Nothing here when using deep sleep
}
