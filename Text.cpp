#include "Text.h"

#include <Arduino.h>

#include "font_data.inc"

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t a) {
  if (a >= 15) {
    return fg;
  }
  const uint8_t fr = (fg >> 11) & 0x1F;
  const uint8_t fg6 = (fg >> 5) & 0x3F;
  const uint8_t fb = fg & 0x1F;
  const uint8_t br = (bg >> 11) & 0x1F;
  const uint8_t bg6 = (bg >> 5) & 0x3F;
  const uint8_t bb = bg & 0x1F;
  const uint8_t inv = 15 - a;
  const uint8_t r = (fr * a + br * inv) / 15;
  const uint8_t g = (fg6 * a + bg6 * inv) / 15;
  const uint8_t b = (fb * a + bb * inv) / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static const Glyph* findGlyph(const FontFace& font, uint16_t code) {
  int lo = 0;
  int hi = (int)font.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) >> 1;
    const uint16_t got = font.glyphs[mid].code;
    if (got == code) {
      return &font.glyphs[mid];
    }
    if (got < code) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return nullptr;
}

static uint16_t nextCode(const char*& s) {
  const uint8_t c = (uint8_t)*s;
  if (c == 0) {
    return 0;
  }
  if (c < 0x80) {
    ++s;
    return c;
  }
  if ((c & 0xE0) == 0xC0 && s[1] != '\0') {
    const uint16_t cp = (uint16_t)(((c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F));
    s += 2;
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && s[1] != '\0' && s[2] != '\0') {
    const uint16_t cp = (uint16_t)(((c & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) |
                                   ((uint8_t)s[2] & 0x3F));
    s += 3;
    return cp;
  }
  ++s;
  return '?';
}

int textWidth(const FontFace& font, const char* utf8) {
  if (utf8 == nullptr) {
    return 0;
  }
  int width = 0;
  const char* p = utf8;
  while (const uint16_t cp = nextCode(p)) {
    const Glyph* glyph = findGlyph(font, cp);
    width += glyph != nullptr ? glyph->advance : font.line_height / 3;
  }
  return width;
}

static void stamp(LGFX& lcd, int pen_x, int top, const Glyph& glyph, const FontFace& font,
                  uint16_t fg, uint16_t bg, int clip_x, int clip_y, int clip_w, int clip_h) {
  if (glyph.width == 0 || glyph.height == 0) {
    return;
  }
  const uint8_t* bits = font.bits + glyph.offset;
  for (uint8_t y = 0; y < glyph.height; ++y) {
    const int py = top + y;
    if (py < clip_y || py >= clip_y + clip_h || py < 0 || py >= SCREEN) {
      continue;
    }
    for (uint8_t x = 0; x < glyph.width; ++x) {
      const int px = pen_x + x;
      if (px < clip_x || px >= clip_x + clip_w || px < 0 || px >= SCREEN) {
        continue;
      }
      const uint32_t pixel = (uint32_t)y * glyph.width + x;
      const uint8_t byte = bits[pixel >> 1];
      const uint8_t alpha = (pixel & 1) ? (byte & 0x0F) : (byte >> 4);
      if (alpha == 0) {
        continue;
      }
      lcd.drawPixel(px, py, blend565(fg, bg, alpha));
    }
  }
}

void drawTextBox(LGFX& lcd, int x, int y, int w, int h, const char* utf8, const FontFace& font,
                 uint16_t fg, uint16_t bg) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (utf8 == nullptr) {
    utf8 = "";
  }
  lcd.startWrite();
  lcd.fillRect(x, y, w, h, bg);
  const int tw = textWidth(font, utf8);
  int pen = x + (w - tw) / 2;
  const int top = y + ((int)h - (int)font.line_height) / 2;
  const char* p = utf8;
  while (const uint16_t cp = nextCode(p)) {
    const Glyph* glyph = findGlyph(font, cp);
    if (glyph == nullptr) {
      pen += font.line_height / 3;
      continue;
    }
    stamp(lcd, pen, top, *glyph, font, fg, bg, x, y, w, h);
    pen += glyph->advance;
  }
  lcd.endWrite();
}

void drawTextCentered(LGFX& lcd, int cx, int cy, const char* utf8, const FontFace& font,
                      uint16_t fg, uint16_t bg) {
  if (utf8 == nullptr || utf8[0] == '\0') {
    return;
  }
  int w = textWidth(font, utf8);
  int h = font.line_height;
  if (w < 1) {
    w = 1;
  }
  if (w > 200) {
    w = 200;
  }
  drawTextBox(lcd, cx - w / 2, cy - h / 2, w, h, utf8, font, fg, bg);
}
