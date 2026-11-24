#include "Face.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Text.h"

struct Gauge {
  uint8_t id;
  char caption[28];
  char unit[12];
  char text[20];
  float frac;
  uint16_t color;
  bool arrow;
  bool arrow_right;
  bool arrow_on;
};

static float clampf(float value) {
  if (value < 0) {
    return 0;
  }
  if (value > 1) {
    return 1;
  }
  return value;
}

static float fractionOf(const ParamDef& def, bool known, int16_t value) {
  if (!known) {
    return 0;
  }
  if (def.id == 0x05) {
    if (value >= '1' && value <= '6') {
      return (float)(value - '0') / 6.f;
    }
    return 0;
  }
  if (def.id == 0x07 || def.id == 0x08) {
    return value != 0 ? 1.f : 0.f;
  }
  if (def.max_v == def.min_v) {
    return 0;
  }
  return clampf((float)(value - def.min_v) / (float)(def.max_v - def.min_v));
}

static uint16_t gaugeColor(const ParamDef& def, float frac) {
  if (def.low_is_bad) {
    if (frac < 0.15f) {
      return COL_RED;
    }
    if (frac < 0.30f) {
      return COL_AMBER;
    }
    return COL_CYAN;
  }
  if (frac > 0.92f) {
    return COL_RED;
  }
  if (frac > 0.75f) {
    return COL_AMBER;
  }
  return COL_CYAN;
}

static void formatValue(const ParamDef& def, bool known, int16_t value, char* out, size_t size) {
  if (!known) {
    strncpy(out, "--", size);
    out[size - 1] = '\0';
    return;
  }
  switch (def.id) {
    case 0x05:
      if (value >= 32 && value < 127) {
        out[0] = (char)value;
        out[1] = '\0';
      } else {
        strncpy(out, "-", size);
      }
      break;
    case 0x06:
      switch (value) {
        case 1:
          strncpy(out, "габ", size);
          break;
        case 2:
          strncpy(out, "ближ", size);
          break;
        case 3:
          strncpy(out, "даль", size);
          break;
        default:
          strncpy(out, "выкл", size);
          break;
      }
      break;
    case 0x07:
    case 0x08:
      strncpy(out, value ? "да" : "нет", size);
      break;
    default:
      snprintf(out, size, "%d", (int)value);
      break;
  }
  out[size - 1] = '\0';
}

static void buildGauge(const Reading& reading, Gauge& gauge) {
  memset(&gauge, 0, sizeof(gauge));
  const ParamDef* def = reading.def != nullptr ? reading.def : paramAt(0);
  gauge.id = def->id;
  snprintf(gauge.caption, sizeof(gauge.caption), "%s", def->title);
  snprintf(gauge.unit, sizeof(gauge.unit), "%s", def->unit);
  gauge.frac = fractionOf(*def, reading.known, reading.value);
  gauge.color = reading.known ? gaugeColor(*def, gauge.frac) : COL_MUTED;
  formatValue(*def, reading.known, reading.value, gauge.text, sizeof(gauge.text));
  if (reading.known && (def->id == 0x07 || def->id == 0x08)) {
    gauge.arrow = true;
    gauge.arrow_right = def->id == 0x08;
    gauge.arrow_on = reading.value != 0;
    gauge.color = gauge.arrow_on ? COL_AMBER : COL_TRACK;
  }
}

static bool asciiText(const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    if ((uint8_t)*p >= 0x80) {
      return false;
    }
  }
  return true;
}

static void paintValue(LGFX& lcd, const Gauge& gauge, int x, int y, int w, int h, uint16_t bg) {
  if (gauge.arrow) {
    lcd.fillRect(x, y, w, h, bg);
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    if (gauge.arrow_right) {
      lcd.fillTriangle(cx - 14, cy - 16, cx - 14, cy + 16, cx + 18, cy, gauge.color);
    } else {
      lcd.fillTriangle(cx + 14, cy - 16, cx + 14, cy + 16, cx - 18, cy, gauge.color);
    }
    return;
  }
  const FontFace* font = &font16;
  if (asciiText(gauge.text) && textWidth(font34, gauge.text) <= w - 6) {
    font = &font34;
  }
  drawTextBox(lcd, x, y, w, h, gauge.text, *font, gauge.color, bg);
}

static void paintMeasure(LGFX& lcd, const Gauge& gauge, int x, int y, int w, int h, uint16_t bg) {
  const char* text = gauge.unit[0] != '\0' ? gauge.unit : gauge.caption;
  drawTextBox(lcd, x, y, w, h, text, font16, COL_MUTED, bg);
}

static void polar(int cx, int cy, float deg, float radius, int& x, int& y) {
  const float rad = deg * 0.0174532925f;
  x = cx + (int)lroundf(cosf(rad) * radius);
  y = cy + (int)lroundf(sinf(rad) * radius);
}

static void ticks(LGFX& lcd, int majors, int subs, int minor_inner, int major_inner, int outer) {
  const int total = majors * subs;
  for (int i = 0; i <= total; ++i) {
    const bool major = (i % subs) == 0;
    const float deg = 135.f + 270.f * ((float)i / (float)total);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    polar(120, 120, deg, major ? major_inner : minor_inner, x0, y0);
    polar(120, 120, deg, outer, x1, y1);
    if (major) {
      lcd.drawWideLine(x0, y0, x1, y1, 1.15f, COL_WHITE);
    } else {
      lcd.drawLine(x0, y0, x1, y1, COL_MUTED);
    }
  }
}

static void needle(LGFX& lcd, float frac, int inner, int outer, float width, uint16_t color) {
  const float deg = 135.f + 270.f * clampf(frac);
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  polar(120, 120, deg, inner, x0, y0);
  polar(120, 120, deg, outer, x1, y1);
  lcd.drawWideLine(x0, y0, x1, y1, width, color);
  lcd.fillCircle(120, 120, width > 1.4f ? 6 : 4, color);
  lcd.fillCircle(120, 120, 2, COL_BG);
}

static void drawIcon(LGFX& lcd, uint8_t id, int cx, int cy, uint16_t color) {
  switch (id) {
    case 0x01: {
      lcd.fillArc(cx, cy + 2, 12, 10, 210, 330, color);
      int x1 = 0;
      int y1 = 0;
      polar(cx, cy + 2, 250, 8, x1, y1);
      lcd.drawWideLine(cx, cy + 2, x1, y1, 0.9f, color);
      break;
    }
    case 0x02:
      lcd.drawRoundRect(cx - 8, cy - 7, 12, 16, 2, color);
      lcd.fillRect(cx - 6, cy - 5, 8, 4, color);
      lcd.drawLine(cx + 4, cy - 3, cx + 9, cy - 9, color);
      lcd.drawLine(cx + 4, cy - 2, cx + 9, cy - 8, color);
      lcd.fillRect(cx + 7, cy - 12, 5, 3, color);
      break;
    case 0x03:
      lcd.drawCircle(cx, cy, 9, color);
      lcd.fillTriangle(cx + 1, cy - 12, cx + 10, cy - 7, cx + 1, cy - 3, color);
      break;
    case 0x04:
      lcd.drawRoundRect(cx - 3, cy - 12, 7, 18, 3, color);
      lcd.fillCircle(cx, cy + 8, 5, color);
      lcd.fillRect(cx - 1, cy - 1, 3, 9, color);
      break;
    case 0x05:
      lcd.drawCircle(cx, cy, 4, color);
      for (int i = 0; i < 8; ++i) {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        polar(cx, cy, i * 45.f, 6, x0, y0);
        polar(cx, cy, i * 45.f, 11, x1, y1);
        lcd.drawWideLine(x0, y0, x1, y1, 1.4f, color);
      }
      break;
    case 0x06:
      lcd.fillCircle(cx - 2, cy, 5, color);
      lcd.drawLine(cx + 4, cy - 6, cx + 11, cy - 10, color);
      lcd.drawLine(cx + 5, cy, cx + 13, cy, color);
      lcd.drawLine(cx + 4, cy + 6, cx + 11, cy + 10, color);
      break;
    case 0x07:
      lcd.fillTriangle(cx + 8, cy - 8, cx + 8, cy + 8, cx - 9, cy, color);
      break;
    case 0x08:
      lcd.fillTriangle(cx - 8, cy - 8, cx - 8, cy + 8, cx + 9, cy, color);
      break;
    default:
      break;
  }
}

static void drawArcNumber(LGFX& lcd, const Gauge& gauge, uint16_t bg) {
  lcd.fillArc(120, 120, 110, 98, 135, 405, COL_TRACK);
  if (gauge.frac > 0.01f) {
    lcd.fillArc(120, 120, 110, 98, 135, 135.f + 270.f * gauge.frac, gauge.color);
  }
  drawIcon(lcd, gauge.id, 120, 78, COL_WHITE);
  paintValue(lcd, gauge, 28, 98, 184, 48, bg);
  paintMeasure(lcd, gauge, 36, 150, 168, 22, bg);
}

static void drawNeedleFace(LGFX& lcd, const Gauge& gauge, uint16_t bg, bool swiss) {
  lcd.fillCircle(120, 120, 112, bg);
  if (swiss) {
    lcd.drawCircle(120, 120, 104, COL_TRACK);
    lcd.drawCircle(120, 120, 96, COL_TRACK);
    ticks(lcd, 12, 5, 100, 92, 110);
  } else {
    lcd.drawCircle(120, 120, 90, COL_TRACK);
    ticks(lcd, 10, 2, 100, 88, 110);
  }
  drawIcon(lcd, gauge.id, 120, 78, COL_WHITE);
  paintValue(lcd, gauge, 46, 128, 148, 40, bg);
  paintMeasure(lcd, gauge, 40, 170, 160, 22, bg);
  if (swiss) {
    needle(lcd, gauge.frac, 8, 90, 0.7f, gauge.color);
  } else {
    needle(lcd, gauge.frac, 10, 86, 1.8f, gauge.color);
  }
}

static void drawDigits(LGFX& lcd, const Gauge& a, const Gauge& b, uint16_t bg) {
  drawIcon(lcd, a.id, 120, 40, COL_WHITE);
  paintValue(lcd, a, 28, 54, 184, 44, bg);
  paintMeasure(lcd, a, 36, 100, 168, 18, bg);
  drawIcon(lcd, b.id, 120, 136, COL_WHITE);
  paintValue(lcd, b, 28, 150, 184, 44, bg);
  paintMeasure(lcd, b, 36, 196, 168, 18, bg);
}

static void drawColumn(LGFX& lcd, const Gauge& gauge, int cx, uint16_t bg) {
  const int x = cx - 16;
  const int y = 80;
  const int h = 90;
  lcd.fillRoundRect(x, y, 32, h, 7, COL_TRACK);
  int fill = (int)lroundf(h * clampf(gauge.frac));
  if (fill > h) {
    fill = h;
  }
  if (fill > 3) {
    lcd.fillRoundRect(x, y + h - fill, 32, fill, 7, gauge.color);
  }
  drawIcon(lcd, gauge.id, cx, 40, COL_WHITE);
  paintMeasure(lcd, gauge, cx - 38, 56, 76, 18, bg);
  paintValue(lcd, gauge, cx - 38, 176, 76, 32, bg);
}

static void drawBars(LGFX& lcd, const Gauge& a, const Gauge& b, uint16_t bg) {
  drawColumn(lcd, a, 78, bg);
  drawColumn(lcd, b, 162, bg);
}

void drawFace(LGFX& lcd, uint8_t theme, const Reading& a, const Reading& b, const char* banner,
              uint16_t rim, bool full) {
  const uint16_t bg = COL_BG;
  Gauge ga;
  Gauge gb;
  buildGauge(a, ga);
  buildGauge(b, gb);
  if (banner != nullptr && banner[0] != '\0' && !a.known) {
    strncpy(ga.text, banner, sizeof(ga.text) - 1);
    ga.text[sizeof(ga.text) - 1] = '\0';
    ga.arrow = false;
    ga.color = COL_WHITE;
    ga.frac = 0;
  }
  if (full) {
    lcd.fillScreen(bg);
  }
  switch (theme) {
    case 1:
      drawNeedleFace(lcd, ga, bg, false);
      break;
    case 2:
      drawNeedleFace(lcd, ga, bg, true);
      break;
    case 3:
      drawDigits(lcd, ga, gb, bg);
      break;
    case 4:
      drawBars(lcd, ga, gb, bg);
      break;
    default:
      drawArcNumber(lcd, ga, bg);
      break;
  }
  lcd.fillArc(120, 120, 119, 114, 0, 360, rim);
}

void drawHoldRing(LGFX& lcd, float fraction) {
  if (fraction < 0.02f) {
    return;
  }
  if (fraction > 1.f) {
    fraction = 1.f;
  }
  lcd.fillArc(120, 120, 119, 111, 270, 270 + 360.f * fraction, COL_RED);
}
