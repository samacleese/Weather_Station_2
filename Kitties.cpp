#include "Kitties.h"

#include "KittyPics.h"

const uint8_t* Kitties::getNextKitty() { return m_kitties[m_count++ % NUM_OF_KITTIES]; }

const uint16_t Kitties::w{KittyPics::w};
const uint16_t Kitties::h{KittyPics::h};
const uint8_t* Kitties::m_kitties[]{
    KittyPics::Rufalina,
    KittyPics::thundercleese,
    KittyPics::sam,
    KittyPics::Pcals2,
    KittyPics::Peaches,
};
RTC_DATA_ATTR uint8_t Kitties::m_count{0};
