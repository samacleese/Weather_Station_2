// ABOUTME: Interface for time access, abstracting millis() for testability.
// ABOUTME: Implement with SystemClock for production; stub for tests.
#ifndef ICLOCK_H
#define ICLOCK_H

class IClock {
   public:
    virtual ~IClock() = default;
    virtual unsigned long millis() const = 0;
};

#endif  // ICLOCK_H
