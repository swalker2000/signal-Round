#pragma once

#include "LGFX_Round.h"
#include "config.h"

struct Reading {
  const ParamDef* def;
  bool known;
  int16_t value;
};

// 0 дуга, 1 стрелка, 2 часы — только параметр 1.
// 3 цифры, 4 столбики — оба параметра.
void drawFace(LGFX& lcd, uint8_t theme, const Reading& a, const Reading& b, const char* banner,
              uint16_t rim, bool full);

void drawHoldRing(LGFX& lcd, float fraction);
