#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// 8x8 font (ASCII 0–127)
static const uint8_t font[95][8] = {
#include "font_data.inc"
};

#endif