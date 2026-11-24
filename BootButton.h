#pragma once

#include <stdint.h>

enum class ButtonEvent : uint8_t { None, Short, Long };

// Кнопка BOOT, GPIO9, на землю. Короткое нажатие и удержание разделены.
class BootButton {
 public:
  void begin();
  ButtonEvent poll();
  float holdFraction() const;

 private:
  bool armed_ = false;
  bool held_ = false;
  bool fired_long_ = false;
  bool raw_ = false;
  bool stable_ = false;
  uint32_t change_ms_ = 0;
  uint32_t down_ms_ = 0;
};
