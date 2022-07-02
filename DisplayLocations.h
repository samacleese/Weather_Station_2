#ifndef DISPLAY_LOCATIONS_H
#define DISPLAY_LOCATIONS_H

#include <libs/Adafruit-GFX-Library/gfxfont.h>
#include <stdint.h>

#include <map>

struct DisplayLocation {
  typedef const std::map<uint8_t, const GFXfont *> *font_map_t;

  DisplayLocation(int16_t x, int16_t y, uint16_t w, uint16_t h);
  void setFont(font_map_t font_map_p);
  void setCenterJustification(bool centered);
  void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy,
                  uint8_t font_size);

 private:
  bool m_centered;
  uint8_t m_fontmax;
  int16_t m_x;
  int16_t m_y;
  uint16_t m_w;
  uint16_t m_h;
  font_map_t m_font_map_p;
};

#endif  // DISPLAY_LOCATIONS_H
