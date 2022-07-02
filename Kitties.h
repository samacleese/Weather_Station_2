#ifndef KITTIES_H
#define KITTIES_H

#include <stdint.h>

#define NUM_OF_KITTIES 2

class Kitties {

private:
  static const uint8_t* m_kitties[NUM_OF_KITTIES];
  static uint8_t m_count;

public:
  static const uint8_t* getNextKitty();
  static const uint16_t w;
  static const uint16_t h;

};

#endif //KITTIES_H
