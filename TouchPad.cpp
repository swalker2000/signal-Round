#include "TouchPad.h"

#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>

#include "config.h"

static const uint8_t CST816_ADDR = 0x15;

void TouchPad::begin() {
  pinMode(PIN_TP_RST, OUTPUT);
  pinMode(PIN_TP_INT, INPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(12);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(50);
}

bool TouchPad::readRaw(int& x, int& y, bool& down) {
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom((int)CST816_ADDR, 6) != 6) {
    return false;
  }
  uint8_t data[6];
  for (int i = 0; i < 6; ++i) {
    data[i] = Wire.read();
  }
  int tx = ((data[2] & 0x0F) << 8) | data[3];
  int ty = ((data[4] & 0x0F) << 8) | data[5];
  if (TOUCH_SWAP_XY) {
    const int swap = tx;
    tx = ty;
    ty = swap;
  }
  if (TOUCH_MIRROR_X) {
    tx = (SCREEN - 1) - tx;
  }
  if (TOUCH_MIRROR_Y) {
    ty = (SCREEN - 1) - ty;
  }
  x = tx;
  y = ty;
  down = data[1] > 0 && tx >= 0 && ty >= 0 && tx < SCREEN && ty < SCREEN;
  return true;
}

TouchEvent TouchPad::poll() {
  TouchEvent event{TouchKind::None, 0, 0};
  const uint32_t now = millis();
  if (now - last_read_ < 25) {
    return event;
  }
  last_read_ = now;

  int x = 0;
  int y = 0;
  bool down = false;
  if (!readRaw(x, y, down)) {
    return event;
  }

  if (down) {
    if (!tracking_) {
      tracking_ = true;
      x0_ = x1_ = x;
      y0_ = y1_ = y;
      down_ms_ = now;
    } else {
      x1_ = x;
      y1_ = y;
    }
    return event;
  }

  if (!tracking_) {
    return event;
  }
  tracking_ = false;
  if (now - last_fire_ < 180) {
    return event;
  }

  const int dx = x1_ - x0_;
  const int dy = y1_ - y0_;
  if (abs(dy) > 28 && abs(dy) > abs(dx) + 8) {
    event.kind = dy < 0 ? TouchKind::SwipeUp : TouchKind::SwipeDown;
    event.x = (int16_t)x0_;
    event.y = (int16_t)y0_;
    last_fire_ = now;
    return event;
  }
  if (abs(dx) < 14 && abs(dy) < 14 && now - down_ms_ > 20 && now - down_ms_ < 700) {
    event.kind = TouchKind::Tap;
    event.x = (int16_t)x0_;
    event.y = (int16_t)y0_;
    last_fire_ = now;
  }
  return event;
}
