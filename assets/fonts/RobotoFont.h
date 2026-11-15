// ABOUTME: Picks a Roboto font face by style and size at compile time, e.g. ROBOTO_FONT(Light, 42).
// ABOUTME: Naming faces directly lets the linker drop every size the firmware never uses.
#pragma once

#include "Roboto_Light.h"
#include "Roboto_Medium.h"

// Expands to &Roboto_<style>_<size>. The size may be a macro such as LAYOUT_MAIN_TEMPERATURE_FONT_SIZE:
// the extra level expands it to its number before pasting. A size with no face is a compile error
// ("'Roboto_Light_13' was not declared"), so add the face to the style's header first.
#define ROBOTO_FONT(style, size) ROBOTO_FONT_FACE(style, size)
#define ROBOTO_FONT_FACE(style, size) (&Roboto_##style##_##size)
