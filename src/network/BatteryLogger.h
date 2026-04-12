// ABOUTME: Posts battery voltage readings to a local JSON store for discharge curve analysis.
// ABOUTME: Appends {ts, raw, adj} entries to a server-side array capped at MAX_ENTRIES.
#ifndef BATTERY_LOGGER_H
#define BATTERY_LOGGER_H

#include <WString.h>
#include <time.h>

class BatteryLogger {
   public:
    explicit BatteryLogger(const String& url);

    // Appends a reading to the remote store. Silently ignores network/server errors.
    void log(time_t timestamp, float rawVoltage, float adjustedVoltage);

   private:
    static const int MAX_ENTRIES = 500;
    String m_url;
};

#endif  // BATTERY_LOGGER_H
