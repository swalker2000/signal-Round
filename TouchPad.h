#pragma once

#include <stdint.h>

enum class TouchKind : uint8_t { None, Tap, SwipeUp, SwipeDown };

struct TouchEvent {
  TouchKind kind;
  int16_t x;
  int16_t y;
};

class TouchPad {
 public:
  void begin();
  TouchEvent poll();

 private:
  bool readRaw(int& x, int& y, bool& down);

  bool tracking_ = false;
  int x0_ = 0;
  int y0_ = 0;
  int x1_ = 0;
  int y1_ = 0;
  uint32_t last_read_ = 0;
  uint32_t down_ms_ = 0;
  uint32_t last_fire_ = 0;
};
