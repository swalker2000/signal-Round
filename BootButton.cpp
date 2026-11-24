#include "BootButton.h"

#include <Arduino.h>

#include "config.h"

void BootButton::begin() {
  pinMode(PIN_BOOT, INPUT_PULLUP);
  raw_ = digitalRead(PIN_BOOT) == LOW;
  stable_ = raw_;
  change_ms_ = millis();
  // Удержание во время сброса не считается: GPIO9 ещё и страп загрузчика.
  armed_ = !stable_;
}

float BootButton::holdFraction() const {
  if (!held_) {
    return 0;
  }
  float fraction = (float)(millis() - down_ms_) / (float)BOOT_LONG_MS;
  if (fraction < 0) {
    fraction = 0;
  }
  if (fraction > 1) {
    fraction = 1;
  }
  return fraction;
}

ButtonEvent BootButton::poll() {
  const bool down = digitalRead(PIN_BOOT) == LOW;
  const uint32_t now = millis();
  if (down != raw_) {
    raw_ = down;
    change_ms_ = now;
  }
  if (now - change_ms_ >= 25 && stable_ != raw_) {
    stable_ = raw_;
  }
  if (!armed_) {
    if (!stable_) {
      armed_ = true;
    }
    return ButtonEvent::None;
  }

  if (stable_) {
    if (!held_) {
      held_ = true;
      fired_long_ = false;
      down_ms_ = now;
    } else if (!fired_long_ && now - down_ms_ >= BOOT_LONG_MS) {
      fired_long_ = true;
      return ButtonEvent::Long;
    }
    return ButtonEvent::None;
  }

  ButtonEvent event = ButtonEvent::None;
  if (held_) {
    const uint32_t held_ms = now - down_ms_;
    if (!fired_long_ && held_ms >= 40 && held_ms < BOOT_LONG_MS) {
      event = ButtonEvent::Short;
    }
    held_ = false;
    fired_long_ = false;
  }
  return event;
}
