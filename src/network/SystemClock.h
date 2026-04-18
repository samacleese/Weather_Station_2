// ABOUTME: Production IClock implementation backed by Arduino's millis().
// ABOUTME: Passed to CurrentConditions in the main sketch.
#ifndef SYSTEMCLOCK_H
#define SYSTEMCLOCK_H

#include "IClock.h"
#include <Arduino.h>

class SystemClock : public IClock {
   public:
    unsigned long millis() const override { return ::millis(); }
};

#endif  // SYSTEMCLOCK_H
