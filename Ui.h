#pragma once

#include "BootButton.h"
#include "Face.h"
#include "LGFX_Round.h"
#include "SettingsStore.h"
#include "SignalClient.h"
#include "TouchPad.h"

class Ui {
 public:
  void begin(LGFX& lcd, const SavedLink& link);
  void openScan(SignalClient& client);
  void loop(ButtonEvent button, TouchEvent touch, float hold, SettingsStore& store,
            SignalClient& client);

 private:
  enum class Screen : uint8_t { Scan, Devices, Param, Dash };

  void finishReset(SettingsStore& store, SignalClient& client);
  void goBack();
  void handleTouch(const TouchEvent& touch, SettingsStore& store, SignalClient& client);
  void chooseParam(int index, SettingsStore& store, SignalClient& client);
  void paint(const SettingsStore& store, const SignalClient& client);
  void paintScan();
  void paintDevices(bool scanning);
  void paintParam();
  void paintDash(const SettingsStore& store, const SignalClient& client);
  void drawRow(int visible, const char* text, uint16_t fg);
  void clampScroll(int total);
  Reading readingFor(uint8_t id, const SignalClient& client) const;

  LGFX* lcd_ = nullptr;
  Screen screen_ = Screen::Scan;
  Peer picked_{};
  Peer shown_[8]{};
  uint8_t shown_n_ = 0;
  uint16_t shown_rev_ = 0;
  uint8_t p1_ = 0x01;
  uint8_t p2_ = 0x03;
  uint8_t theme_ = 0;
  uint8_t step_ = 1;
  int scroll_ = 0;
  bool dirty_ = true;
  bool reset_armed_ = false;
  bool hold_drawn_ = false;
  bool hinted_ = false;
  const char* toast_ = nullptr;
  uint32_t toast_until_ = 0;
  uint32_t scan_ms_ = 0;
  uint32_t spin_ms_ = 0;
  int16_t last_a_ = 0;
  int16_t last_b_ = 0;
  bool last_ka_ = false;
  bool last_kb_ = false;
  uint8_t last_link_ = 0xFF;
  uint16_t last_rim_ = 0;
};
