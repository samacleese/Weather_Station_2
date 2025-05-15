#include "DisplayLocations.h"

#include <ArduinoLog.h>
#include <pgmspace.h>

// Many (but maybe not all) non-AVR board installs define macros
// for compatibility with existing PROGMEM-reading AVR code.
// Do our own checks and defines here for good measure...

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

// Pointers are a peculiar case...typically 16-bit on AVR boards,
// 32 bits elsewhere.  Try to accommodate both...

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
#ifdef __AVR__
    return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
#else
    // expression in __AVR__ section may generate "dereferencing type-punned
    // pointer will break strict-aliasing rules" warning In fact, on other
    // platforms (such as STM32) there is no need to do this pointer magic as
    // program memory may be read in a usual way So expression may be simplified
    return gfxFont->glyph + c;
#endif  //__AVR__
}

inline uint8_t *pgm_read_bitmap_ptr(const GFXfont *gfxFont) {
#ifdef __AVR__
    return (uint8_t *)pgm_read_pointer(&gfxFont->bitmap);
#else
    // expression in __AVR__ section generates "dereferencing type-punned pointer
    // will break strict-aliasing rules" warning In fact, on other platforms (such
    // as STM32) there is no need to do this pointer magic as program memory may
    // be read in a usual way So expression may be simplified
    return gfxFont->bitmap;
#endif  //__AVR__
}

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

namespace {
uint8_t getValidFontSize(DisplayLocation::font_map_t font_map_p, uint8_t max_size) {
    while (max_size != 0) {
        if (font_map_p->find(max_size) != font_map_p->end()) return max_size;
        --max_size;
    }
}
}  // namespace

DisplayLocation::DisplayLocation(int16_t x, int16_t y, uint16_t w, uint16_t h)
    : m_centered(false), m_fontmax(255), m_x(x), m_y(y), m_w(w), m_h(h), m_font_map_p(nullptr) {}

void DisplayLocation::setFont(DisplayLocation::font_map_t font_map_p) { m_font_map_p = font_map_p; }

void DisplayLocation::setCenterJustification(bool centered) { m_centered = centered; }

void DisplayLocation::charBounds(unsigned char c,
                                 int16_t *x,
                                 int16_t *y,
                                 int16_t *minx,
                                 int16_t *miny,
                                 int16_t *maxx,
                                 int16_t *maxy,
                                 uint8_t font_size) {
    if (m_font_map_p) {
        auto gfxFont_p = m_font_map_p->find(font_size);
        int16_t textsize_x = 1;
        int16_t textsize_y = 1;
        bool wrap = true;
        if (gfxFont_p == m_font_map_p->end()) {
            Log.warning(F("Unable to find font size %u" CR), font_size);
            return;
        }

        auto gfxFont = gfxFont_p->second;
        if (c == '\n') {  // Newline?
            *x = 0;       // Reset x to zero, advance y by one line
            *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        } else if (c != '\r') {  // Not a carriage return; is normal char
            uint8_t first = pgm_read_byte(&gfxFont->first), last = pgm_read_byte(&gfxFont->last);
            if ((c >= first) && (c <= last)) {  // Char present in this font?
                GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);
                uint8_t gw = pgm_read_byte(&glyph->width), gh = pgm_read_byte(&glyph->height),
                        xa = pgm_read_byte(&glyph->xAdvance);
                int8_t xo = pgm_read_byte(&glyph->xOffset), yo = pgm_read_byte(&glyph->yOffset);
                if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > m_w)) {
                    *x = 0;  // Reset x to zero, advance y by one line
                    *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
                }
                int16_t tsx = (int16_t)textsize_x, tsy = (int16_t)textsize_y, x1 = *x + xo * tsx, y1 = *y + yo * tsy,
                        x2 = x1 + gw * tsx - 1, y2 = y1 + gh * tsy - 1;
                if (x1 < *minx) *minx = x1;
                if (y1 < *miny) *miny = y1;
                if (x2 > *maxx) *maxx = x2;
                if (y2 > *maxy) *maxy = y2;
                *x += xa * tsx;
            }
        }

    } else {
        Log.warning(F("No Font Currently Set!" CR));
    }
}
