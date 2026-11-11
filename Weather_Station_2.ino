// ABOUTME: Main firmware entry point for the Weather Station.
// ABOUTME: Initializes hardware, fetches weather, renders display, then deep-sleeps.
#include <ArduinoLog.h>
#include <Inkplate.h>
#include <inttypes.h>
#include <rom/rtc.h>

#include <cmath>
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

#include "weather_station_2/user_settings.h"
#include "weather_station_2/layout.h"
#include "weather_station_2/version.h"
#include "src/network/IClock.h"
#include "src/network/IHttpClient.h"
#include "src/network/SystemClock.h"
#include "src/network/CurrentConditions.h"
#include "src/network/PrecipForecast.h"
#include "src/display/DisplayLocations.h"
#include "assets/fonts/Roboto_Light.h"
#include "assets/fonts/Roboto_Medium.h"
#include "src/display/Kitties.h"
#include "src/display/KittyPics.h"
#include "src/display/Snowflake.h"
#include "src/network/BatteryLogger.h"
#include "src/network/Network.h"
#include "src/display/BatteryMeter.h"
#include "src/display/InkplateBatteryReader.h"
#include "src/ota/EcdsaImageVerifier.h"
#include "src/ota/EspOtaPartition.h"
#include "src/ota/FirmwareInstaller.h"
#include "src/ota/HttpFirmwareSource.h"
#include "src/ota/OTAManager.h"
#include "src/ota/UpdateConfirmation.h"

#define US_PER_SEC 1000000ull
#define TIME_TO_SLEEP_S 600
#define ERROR_TIME_TO_SLEEP_S 300

static const uint64_t COMPILED_VERSION = FIRMWARE_VERSION;

// Boots in 1-bit so the splash and OTA screens can use partialUpdate(), which 3-bit mode does not support.
// setup() switches to 3-bit before drawing the weather screen.
Inkplate display(INKPLATE_1BIT);

char buffer[256];

// Horizontal gap between forecast days, and between a percent and its snowflake.
// Day gap is sized so the worst case ("Today 100%" + three "Wed 100%", each with a snowflake,
// at 42pt from x=20) is 1190 px and fits the 1200 px panel.
static const int16_t PRECIP_DAY_GAP = 14;
static const int16_t SNOWFLAKE_GAP = 4;
// Color 0 is black in INKPLATE_3BIT mode (matches setTextColor(0, 7) below).
static const uint16_t INK_BLACK = 0;
static const uint8_t WAKE_BUTTON_PIN = 36;
static const unsigned long BUTTON_POLL_MS = 100;
// Long enough to read a refusal or failure before the weather screen replaces it.
static const unsigned long OTA_MESSAGE_HOLD_MS = 5000;
static const int PROGRESS_STEP_PERCENT = 10;

// Draws "Precip:  Today 1%  Tue 0%  Wed 87%❄ ..." along the bottom of the screen.
static void drawPrecipForecast(const PrecipForecast& forecast, int result) {
    display.setFont(Roboto_Medium.at(LAYOUT_MAIN_PRECIP_FORECAST_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_PRECIP_FORECAST_X, LAYOUT_MAIN_PRECIP_FORECAST_Y);
    if (result != PRECIP_FORECAST_OK) {
        display.print("Precip forecast unavailable");
        return;
    }

    display.print("Precip:");
    char dayText[16];
    for (int i = 0; i < forecast.dayCount; i++) {
        display.setCursor(display.getCursorX() + PRECIP_DAY_GAP, LAYOUT_MAIN_PRECIP_FORECAST_Y);
        PrecipForecast::formatDay(forecast.days[i], dayText, sizeof(dayText));
        display.print(dayText);
        if (forecast.days[i].snow) {
            // GFX font cursor y is the text baseline; sit the icon on it.
            int16_t x = display.getCursorX() + SNOWFLAKE_GAP;
            display.drawBitmap(x, LAYOUT_MAIN_PRECIP_FORECAST_Y - Snowflake::h, Snowflake::bitmap, Snowflake::w,
                               Snowflake::h, INK_BLACK);
            display.setCursor(x + Snowflake::w, LAYOUT_MAIN_PRECIP_FORECAST_Y);
        }
    }
}

// Draws one line of OTA text on the 1-bit splash without refreshing the panel. Clears the line's band first:
// yAdvance above the baseline covers ascenders, and a third of it below covers descenders.
static void drawOtaLine(const GFXfont* font, int16_t x, int16_t y, const char* text) {
    display.fillRect(0, y - font->yAdvance, display.width(), font->yAdvance + font->yAdvance / 3, WHITE);
    display.setFont(font);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(x, y);
    display.print(text);
}

// Refreshes only the changed pixels. Logged because each refresh blocks the download it reports on.
static void refreshOtaScreen() {
    unsigned long startMs = millis();
    display.partialUpdate();
    Log.notice(F("OTA screen refresh took %lu ms" CR), millis() - startMs);
}

static void showOtaStatus(const char* text) {
    drawOtaLine(Roboto_Light.at(LAYOUT_LOADING_OTA_STATUS_FONT_SIZE), LAYOUT_LOADING_OTA_STATUS_X,
                LAYOUT_LOADING_OTA_STATUS_Y, text);
    refreshOtaScreen();
}

static int lastShownProgressPercent = 0;

// FirmwareInstaller progress callback: redraws only every PROGRESS_STEP_PERCENT points and at the end.
static void showDownloadProgress(size_t received, size_t total) {
    if (total == 0) return;
    int percent = (int)((uint64_t)received * 100 / total);
    if (received < total && percent < lastShownProgressPercent + PROGRESS_STEP_PERCENT) return;
    lastShownProgressPercent = percent;
    snprintf(buffer, sizeof(buffer), "Downloading %d%%... Do not power off", percent);
    showOtaStatus(buffer);
}

// Offers newer firmware when the manifest has one. Returns when the weather cycle should carry on; a
// successful install restarts the device instead of returning.
static void offerFirmwareUpdate(IHttpClient& http, const IBatteryReader& batteryReader) {
    OTAManager otaManager(http, COMPILED_VERSION);
    OTAManager::UpdateInfo info;
    if (!otaManager.checkForUpdate(OTA_MANIFEST_URL, info)) return;

    snprintf(buffer, sizeof(buffer), "Update %" PRIu64 " ready. Hold WAKE 2s to install.", info.version);
    Log.notice(F("%s" CR), buffer);
    drawOtaLine(Roboto_Medium.at(LAYOUT_LOADING_OTA_PROMPT_FONT_SIZE), LAYOUT_LOADING_OTA_PROMPT_X,
                LAYOUT_LOADING_OTA_PROMPT_Y, buffer);
    snprintf(buffer, sizeof(buffer), "Battery %d%%. Skips in 30s if not held.",
             BatteryMeter::voltageToPercent(batteryReader.readVoltage()));
    showOtaStatus(buffer);

    pinMode(WAKE_BUTTON_PIN, INPUT);  // input-only pin with an external pull-up on the Inkplate 10
    UpdateConfirmation confirmation;
    UpdateConfirmation::Result result;
    while ((result = confirmation.sample(digitalRead(WAKE_BUTTON_PIN) == LOW, millis())) ==
           UpdateConfirmation::WAITING) {
        delay(BUTTON_POLL_MS);
    }
    if (result == UpdateConfirmation::TIMED_OUT) {
        Log.notice(F("OTA update skipped: WAKE was not held" CR));
        return;
    }

    // This reading, not the one on the prompt, is the one that protects the flash.
    float voltage = batteryReader.readVoltage();
    if (!batteryAllowsInstall(voltage, info.min_battery_mv)) {
        snprintf(buffer, sizeof(buffer), "Battery too low to update (%ld mV, needs %d mV)",
                 std::lround(voltage * 1000.0f), info.min_battery_mv);
        Log.warning(F("%s" CR), buffer);
        showOtaStatus(buffer);
        delay(OTA_MESSAGE_HOLD_MS);
        return;
    }

    // Static keeps HTTPClient and the mbedtls context off the 8KB loop-task stack.
    static HttpFirmwareSource source;
    static EspOtaPartition partition;
    static EcdsaImageVerifier verifier;
    FirmwareInstaller installer(source, partition, verifier);

    lastShownProgressPercent = 0;
    showOtaStatus("Downloading 0%... Do not power off");
    if (installer.install(info, showDownloadProgress)) {
        Log.notice(F("OTA update installed, restarting" CR));
        showOtaStatus("Update installed. Rebooting...");
        esp_restart();
    }

    Log.error(F("OTA install failed" CR));
    showOtaStatus("Update failed. Continuing to weather.");
    delay(OTA_MESSAGE_HOLD_MS);
}

void setup() {
    InkplateBatteryReader batteryReader(display);
    auto network = std::make_shared<WeatherNetwork>(WIFI_SSID, WIFI_PASSWORD);
    rtc_get_reset_reason(0);
    DisplayLocation kitties(850, 50, 300, 300);
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Log.setShowLevel(true);
    Log.notice(F("Firmware version: %s" CR), GIT_VERSION);

    display.begin();

    // Initialize display with loading message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);  // 1-bit colors; 0 would be white in this mode
    display.setFont(Roboto_Medium.at(LAYOUT_LOADING_MESSAGE_FONT_SIZE));
    display.setCursor(LAYOUT_LOADING_MESSAGE_X, LAYOUT_LOADING_MESSAGE_Y);
    display.print("Weather Station Loading...");
    display.setFont(Roboto_Light.at(LAYOUT_LOADING_VERSION_FONT_SIZE));
    display.setCursor(LAYOUT_LOADING_VERSION_X, LAYOUT_LOADING_VERSION_Y);
    display.print(GIT_VERSION);
    display.display();

    network->begin();
    offerFirmwareUpdate(*network, batteryReader);

    SystemClock clock;
    CurrentConditions curr(*network, clock, WEATHER_STATION_ID);

    const uint8_t* nextKitty = Kitties::getNextKitty();

    // Update weather with retry capability
    int updateResult = curr.update(3);  // Try up to 3 times

    display.selectDisplayMode(INKPLATE_3BIT);
    display.clearDisplay();

    // Handle error cases with user feedback
    if (updateResult != CURRENT_CONDITIONS_OK) {
        // Show error message on display
        display.setTextSize(1);
        display.setTextColor(0, 7);
        display.setFont(Roboto_Light.at(14));
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
            display.image.drawBitmap3Bit(850, 50, nextKitty, Kitties::w, Kitties::h);

            // Get and show battery info at least
            int temperature = display.readTemperature();
            float voltage = batteryReader.readVoltage();

            display.setFont(Roboto_Light.at(42));
            display.setCursor(860, 400);
            display.print(BatteryMeter::voltageToPercent(voltage));
            display.print('%');
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

    PrecipForecast forecast(*network, WEATHER_FORECAST_GRIDPOINT);
    int forecastResult = forecast.update();
    Log.notice(F("Free heap after forecast: %u, largest block: %u" CR), ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // Draw Temperature with validity check
    display.setTextSize(1);
    display.setTextColor(0, 7);
    display.setFont(Roboto_Medium.at(LAYOUT_MAIN_TEMPERATURE_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_TEMPERATURE_X, LAYOUT_MAIN_TEMPERATURE_Y);
    if (curr.temperature != -999) {
        sprintf(buffer, "%d C", curr.temperature);
    } else {
        sprintf(buffer, "-- C");
    }
    display.print(buffer);

    // Draw Windspeed with validity check
    display.setFont(Roboto_Medium.at(LAYOUT_MAIN_WIND_SPEED_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_WIND_SPEED_X, LAYOUT_MAIN_WIND_SPEED_Y);
    if (curr.wind_speed != -999) {
        sprintf(buffer, "%d km/h", curr.wind_speed);
    } else {
        sprintf(buffer, "-- km/h");
    }
    display.print(buffer);

    // Weather description
    display.setFont(Roboto_Light.at(LAYOUT_MAIN_DESCRIPTION_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_DESCRIPTION_X, LAYOUT_MAIN_DESCRIPTION_Y);
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

    drawPrecipForecast(forecast, forecastResult);

    // Draw kitty
    display.image.drawBitmap3Bit(LAYOUT_MAIN_KITTY_X, LAYOUT_MAIN_KITTY_Y, nextKitty, Kitties::w, Kitties::h);

    // Get the temp and battery voltage
    int temperature;
    float voltage;

    temperature = display.readTemperature();
    voltage = batteryReader.readVoltage();

    // Display system info
    display.setFont(Roboto_Light.at(LAYOUT_MAIN_BATTERY_INFO_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_BATTERY_INFO_X, LAYOUT_MAIN_BATTERY_INFO_Y);
    display.print(BatteryMeter::voltageToPercent(voltage));
    display.print('%');
    display.print(' ');
    display.print(temperature, DEC);
    display.print('C');

    // If there was a warning but we have data, show it
    if (updateResult != CURRENT_CONDITIONS_OK) {
        display.setFont(Roboto_Light.at(LAYOUT_MAIN_WARNINGS_FONT_SIZE));
        display.setCursor(LAYOUT_MAIN_WARNINGS_X, LAYOUT_MAIN_WARNINGS_Y);
        display.print("Warning: ");
        display.print(curr.getErrorString(updateResult));
    }

    display.display();

    // Log battery reading for discharge curve calibration
    time_t now;
    time(&now);
    BatteryLogger batteryLogger(BATTERY_LOGGER_URL);
    batteryLogger.log(now, voltage);

    // Goto deep sleep
    rtc_gpio_isolate(GPIO_NUM_12);
    esp_sleep_enable_ext1_wakeup((1ULL << 36), ESP_EXT1_WAKEUP_ALL_LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_S * US_PER_SEC);
    esp_deep_sleep_start();
}

void loop() {
    // Nothing here when using deep sleep
}
