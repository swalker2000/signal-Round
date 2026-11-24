#pragma once

#include <stdint.h>

#include "LGFX_Round.h"
#include "font.h"

int textWidth(const FontFace& font, const char* utf8);

// Прямоугольник заливается цветом bg, текст по центру. Так стирается прошлое значение.
void drawTextBox(LGFX& lcd, int x, int y, int w, int h, const char* utf8, const FontFace& font,
                 uint16_t fg, uint16_t bg);

void drawTextCentered(LGFX& lcd, int cx, int cy, const char* utf8, const FontFace& font,
                      uint16_t fg, uint16_t bg);
