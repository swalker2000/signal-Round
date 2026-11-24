#pragma once

#include <stdint.h>

// ESP32-2424S012: круглый GC9A01 240x240, сенсор CST816, кнопка BOOT на GPIO9.
// Если нажатия мимо кнопок, поменяйте зеркала сенсора и прошейте снова.

static const int PIN_LCD_SCLK = 6;
static const int PIN_LCD_MOSI = 7;
static const int PIN_LCD_DC = 2;
static const int PIN_LCD_CS = 10;
static const int PIN_LCD_BL = 3;
static const int PIN_TP_SDA = 4;
static const int PIN_TP_SCL = 5;
static const int PIN_TP_RST = 1;
static const int PIN_TP_INT = 0;
static const int PIN_BOOT = 9;

static const int TOUCH_SWAP_XY = 0;
static const int TOUCH_MIRROR_X = 0;
static const int TOUCH_MIRROR_Y = 0;

static const int SCREEN = 240;
static const uint32_t BOOT_LONG_MS = 1300;
static const uint8_t THEME_COUNT = 5;
static const uint8_t PARAM_COUNT = 8;

static const char* NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static const uint16_t COL_BG = 0x0000;
static const uint16_t COL_WHITE = 0xFFFF;
static const uint16_t COL_MUTED = 0x7BEF;
static const uint16_t COL_CYAN = 0x07FF;
static const uint16_t COL_AMBER = 0xFD20;
static const uint16_t COL_RED = 0xF800;
static const uint16_t COL_GREEN = 0x07E0;
static const uint16_t COL_BLUE = 0x035F;
static const uint16_t COL_TRACK = 0x18C3;
static const uint16_t COL_ROW = 0x1082;
static const uint16_t COL_SPORT = 0x1800;

struct ParamDef {
  uint8_t id;
  const char* title;
  const char* unit;
  int16_t min_v;
  int16_t max_v;
  bool low_is_bad;
};

struct SavedLink {
  bool ready;
  char name[24];
  char addr[18];
  uint8_t addr_type;
  uint8_t param1;
  uint8_t param2;
  uint8_t theme;
};

const ParamDef* paramById(uint8_t id);
const ParamDef* paramAt(uint8_t index);
const char* themeName(uint8_t theme);
