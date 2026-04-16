// ABOUTME: Calibration sketch for Inkplate 10 grayscale levels.
// ABOUTME: Cycles through all 8 shades on button press for luminance measurement.

#include <Arduino.h>
#include <Inkplate.h>

#define WAKEUP_BUTTON_PIN 36

Inkplate display(INKPLATE_3BIT);

static int shade = 0;

void setup() {
    display.begin();
    pinMode(WAKEUP_BUTTON_PIN, INPUT);
}

void loop() {
    display.clearDisplay();
    display.fillScreen(shade);

    // Contrasting text so the label is always legible against any shade
    display.setTextColor(7 - shade);
    display.setTextSize(5);
    display.setCursor(50, 50);
    display.print("Shade: ");
    display.print(shade);
    display.print(" / 7");

    display.setTextSize(2);
    display.setCursor(50, 180);
    display.print("Press button to advance");

    display.display();

    // Wait for wakeup button (GPIO 36 goes LOW on press)
    while (digitalRead(WAKEUP_BUTTON_PIN) == HIGH) {
        delay(50);
    }
    delay(200);  // debounce

    shade = (shade + 1) % 8;
}
