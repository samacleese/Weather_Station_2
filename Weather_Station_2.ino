#include <ArduinoLog.h>
#include <Inkplate.h>
#include <rom/rtc.h>  // Include ESP32 library for RTC (needed for rtc_get_reset_reason() function)

#include <memory>

#include "driver/rtc_io.h"  // Include ESP32 library for RTC pin I/O (needed for rtc_gpio_isolate() function)

// TODO - move this to a seperate header file to add make_unique
namespace std {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace std

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
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
    display.setTextSize(3);
    display.setTextColor(0, 7);
    display.setCursor(150, 70);
    sprintf(buffer, "Rufalina Address = %p", KittyPics::Rufalina);
    display.print(buffer);

    display.setCursor(150, 170);
    sprintf(buffer, "Thundercleese Address = %p", KittyPics::thundercleese);
    display.print(buffer);

    display.setCursor(150, 270);
    sprintf(buffer, "Sam Address = %p", KittyPics::sam);
    display.print(buffer);

    display.setCursor(150, 370);
    sprintf(buffer, "Pcals Address = %p", KittyPics::Pcals);
    display.print(buffer);

    display.setCursor(150, 470);
    sprintf(buffer, "Peaches Address = %p", KittyPics::Peaches);
    display.print(buffer);

    display.setCursor(150, 570);
    sprintf(buffer, (const char*)F("Next Kitty Address = %p"), Kitties::getNextKitty());
    display.print(buffer);

    display.display();
    network->begin();

    CurrentConditions curr(network);

    const uint8_t* nextKitty = Kitties::getNextKitty();

    curr.update();

    display.clearDisplay();

    // Draw Tempature
    display.setTextSize(1);
    display.setTextColor(0, 7);
    display.setFont(Roboto_Medium.at(150));
    display.setCursor(50, 150);
    sprintf(buffer, "%d C", curr.temperature);
    display.print(buffer);

    // Draw Windspeed
    display.setCursor(50, 325);
    sprintf(buffer, "%d km/h", curr.wind_speed);
    display.print(buffer);

    // Raw Message
    display.setFont(Roboto_Medium.at(20));
    display.setCursor(20, 750);
    sprintf(buffer, "%s", curr.raw_message);
    display.print(buffer);

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

    display.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);

    // Get the temp and battery voltage
    int temperature;
    float voltage;

    temperature = display.readTemperature();  // Read temperature from on-board temperature sensor
    voltage =
        display.readBattery();  // Read battery voltage (NOTE: Doe to ESP32 ADC accuracy, you should calibrate the ADC!)

    voltage += ADC_OFFSET;

    //    display.drawImage(battSymbol, 100, 100, 106, 45, BLACK); // Draw battery symbol at position X=100 Y=100
    display.setFont(Roboto_Light.at(42));
    display.setCursor(860, 400);
    display.print(voltage, 2);  // Print battery voltage
    display.print('V');
    display.print(' ');

    display.print(temperature, DEC);  // Print temperature
    display.print('C');

    display.display();

    // Goto deep sleep
    rtc_gpio_isolate(GPIO_NUM_12);  // Isolate/disable GPIO12 on ESP32 (only to reduce power consumption in sleep)
    // Enable wakup from deep sleep on gpio 36
    esp_sleep_enable_ext1_wakeup((1ULL << 36), ESP_EXT1_WAKEUP_ALL_LOW);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_S * US_PER_SEC);
    esp_deep_sleep_start();
}

void loop() {
    // Nothing here when using deep sleep
}
