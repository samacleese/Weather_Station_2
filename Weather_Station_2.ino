#include <Inkplate.h>

#include <ArduinoLog.h>

#include <memory>

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
#include "DisplayLocations.h"
#include "Kitties.h"
#include "KittyPics.h"
#include "Network.h"
#include "CurrentConditions.h"


#include "Fonts/Roboto_Light.h"
#include "Fonts/Roboto_Medium.h"

Inkplate display(INKPLATE_3BIT);
auto network = std::make_shared<Network>("NorwegianFish", "rufalina");


char buffer[256];

void setup() {
  DisplayLocation kitties(850, 50, 300, 300);
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.setShowLevel(true);

  display.begin();
  display.setTextSize(3);
  display.setTextColor(0, 7);
  display.setCursor(150, 370);
  sprintf(buffer, "Sam Address = %p", KittyPics::sam);
  display.print(buffer);

  display.setCursor(150, 470);
  sprintf(buffer, "Thundercleese Address = %p", KittyPics::thundercleese);
  display.print(buffer);

  display.setCursor(150, 570);
  sprintf(buffer, (const char*)F("Next Kitty Address = %p"), Kitties::getNextKitty());
  display.print(buffer);

  display.display();
  network->begin();
  delay(5000);
}

void loop() {

  while (0) { 
    const uint8_t* nextKitty = Kitties::getNextKitty();
    display.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);
    display.display();
	delay(50000); 
	}

  CurrentConditions curr(network);

  while (1) {
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
        x = 100;
        y = 720;
        display.getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
        Log.info(F("Min X = %d, Min Y = %d, Max X = %ud, Max y = %ud" CR), x1, y1, w, h);
    }
    display.print(buffer);

    display.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);
    display.display();
    delay(300000);
  }
}
