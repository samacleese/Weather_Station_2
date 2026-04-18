#ifndef KITTIES_H
#define KITTIES_H

#include <stdint.h>

#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif

#define NUM_OF_KITTIES 5

class Kitties {
   private:
    static const uint8_t* m_kitties[NUM_OF_KITTIES];
    static uint8_t m_count;

   public:
    static const uint8_t* getNextKitty();
    static const uint16_t w;
    static const uint16_t h;
};

#endif  // KITTIES_H
