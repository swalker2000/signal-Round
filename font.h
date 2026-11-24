#pragma once

#include <stdint.h>

struct Glyph {
  uint16_t code;
  uint8_t width;
  uint8_t height;
  uint8_t advance;
  uint32_t offset;
};

struct FontFace {
  uint8_t line_height;
  uint16_t count;
  const Glyph* glyphs;
  const uint8_t* bits;
};

extern const FontFace font16;
extern const FontFace font34;
