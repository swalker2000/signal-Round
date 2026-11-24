#include "Ui.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "Text.h"

static const int kRowX = 42;
static const int kRowY = 48;
static const int kRowW = 156;
static const int kRowH = 30;
static const int kRowPitch = 34;

static bool inside(int x, int y, int left, int top, int width, int height) {
  return x >= left && y >= top && x < left + width && y < top + height;
}

static int rowAt(int x, int y) {
  for (int i = 0; i < 4; ++i) {
    if (inside(x, y, kRowX, kRowY + i * kRowPitch, kRowW, kRowH)) {
      return i;
    }
  }
  return -1;
}

void Ui::begin(LGFX& lcd, const SavedLink& link) {
  lcd_ = &lcd;
  theme_ = link.theme < THEME_COUNT ? link.theme : 0;
  screen_ = link.ready ? Screen::Dash : Screen::Scan;
  dirty_ = true;
}

void Ui::openScan(SignalClient& client) {
  screen_ = Screen::Scan;
  scroll_ = 0;
  dirty_ = true;
  scan_ms_ = millis();
  client.startScan();
}

void Ui::finishReset(SettingsStore& store, SignalClient& client) {
  store.clear();
  p1_ = 0x01;
  p2_ = 0x03;
  theme_ = 0;
  step_ = 1;
  hinted_ = false;
  toast_ = nullptr;
  memset(&picked_, 0, sizeof(picked_));
  reset_armed_ = false;
  hold_drawn_ = false;
  openScan(client);
}

void Ui::goBack() {
  if (screen_ != Screen::Param) {
    return;
  }
  if (step_ == 2) {
    step_ = 1;
  } else {
    screen_ = Screen::Devices;
  }
  scroll_ = 0;
  dirty_ = true;
}

void Ui::clampScroll(int total) {
  const int window = total > 4 ? 3 : total;
  int max_scroll = total - window;
  if (max_scroll < 0) {
    max_scroll = 0;
  }
  if (scroll_ < 0) {
    scroll_ = 0;
  }
  if (scroll_ > max_scroll) {
    scroll_ = max_scroll;
  }
}

void Ui::drawRow(int visible, const char* text, uint16_t fg) {
  const int top = kRowY + visible * kRowPitch;
  lcd_->fillRoundRect(kRowX, top, kRowW, kRowH, 8, COL_ROW);
  drawTextCentered(*lcd_, 120, top + kRowH / 2, text, font16, fg, COL_ROW);
}

Reading Ui::readingFor(uint8_t id, const SignalClient& client) const {
  Reading reading;
  reading.def = paramById(id);
  if (reading.def == nullptr) {
    reading.def = paramAt(0);
  }
  reading.known = client.value(reading.def->id, reading.value);
  return reading;
}

void Ui::chooseParam(int index, SettingsStore& store, SignalClient& client) {
  if (index == PARAM_COUNT) {
    goBack();
    return;
  }
  const ParamDef* param = paramAt((uint8_t)index);
  if (param == nullptr) {
    return;
  }
  if (step_ == 2 && param->id == p1_) {
    return;
  }
  if (step_ == 1) {
    p1_ = param->id;
    step_ = 2;
    scroll_ = 0;
    dirty_ = true;
    return;
  }
  p2_ = param->id;
  SavedLink link{};
  link.ready = true;
  strncpy(link.name, picked_.name, sizeof(link.name) - 1);
  strncpy(link.addr, picked_.addr, sizeof(link.addr) - 1);
  link.addr_type = picked_.type;
  link.param1 = p1_;
  link.param2 = p2_;
  link.theme = theme_;
  store.save(link);
  client.connectSaved(store.get());
  screen_ = Screen::Dash;
  toast_ = nullptr;
  dirty_ = true;
}

void Ui::handleTouch(const TouchEvent& touch, SettingsStore& store, SignalClient& client) {
  if (touch.kind == TouchKind::None || screen_ == Screen::Dash || screen_ == Screen::Scan) {
    return;
  }

  const int total = screen_ == Screen::Devices ? (shown_n_ == 0 ? 1 : (int)shown_n_ + 1) : PARAM_COUNT + 1;
  if (touch.kind == TouchKind::SwipeUp || touch.kind == TouchKind::SwipeDown) {
    if (total > 4) {
      scroll_ += touch.kind == TouchKind::SwipeUp ? 1 : -1;
      clampScroll(total);
      dirty_ = true;
    }
    return;
  }
  if (touch.kind != TouchKind::Tap) {
    return;
  }

  if (screen_ == Screen::Devices && shown_n_ == 0) {
    if (rowAt(touch.x, touch.y) == 2) {
      openScan(client);
    }
    return;
  }

  const int visible = rowAt(touch.x, touch.y);
  if (visible < 0) {
    return;
  }
  if (total > 4 && visible == 3) {
    if (scroll_ + 3 < total) {
      scroll_ += 3;
    } else {
      scroll_ = 0;
    }
    clampScroll(total);
    dirty_ = true;
    return;
  }
  const int index = scroll_ + visible;
  if (index < 0 || index >= total) {
    return;
  }
  if (screen_ == Screen::Devices) {
    if (index == (int)shown_n_) {
      openScan(client);
      return;
    }
    picked_ = shown_[index];
    step_ = 1;
    scroll_ = 0;
    screen_ = Screen::Param;
    dirty_ = true;
    return;
  }
  chooseParam(index, store, client);
}

void Ui::paintScan() {
  const uint32_t now = millis();
  if (dirty_) {
    lcd_->fillScreen(COL_BG);
    drawTextCentered(*lcd_, 120, 36, "Поиск", font16, COL_WHITE, COL_BG);
    char line[24];
    snprintf(line, sizeof(line), "найдено %u", shown_n_);
    drawTextCentered(*lcd_, 120, 58, line, font16, COL_MUTED, COL_BG);
    drawTextCentered(*lcd_, 120, 174, "кнопка - назад", font16, COL_MUTED, COL_BG);
    drawTextCentered(*lcd_, 120, 196, "удержание - сброс", font16, COL_MUTED, COL_BG);
    dirty_ = false;
    spin_ms_ = 0;
  }
  if (now - spin_ms_ < 70) {
    return;
  }
  spin_ms_ = now;
  const float angle = (float)((now / 6) % 360);
  lcd_->fillArc(120, 120, 46, 38, 0, 360, COL_TRACK);
  lcd_->fillArc(120, 120, 46, 38, angle, angle + 70.f, COL_CYAN);
}

void Ui::paintDevices(bool scanning) {
  if (!dirty_) {
    return;
  }
  lcd_->fillScreen(COL_BG);
  drawTextCentered(*lcd_, 120, 26, "Устройства", font16, COL_WHITE, COL_BG);
  if (shown_n_ == 0) {
    drawTextCentered(*lcd_, 120, 96, scanning ? "Ищем..." : "Никого нет", font16, COL_MUTED, COL_BG);
    drawRow(2, "Обновить", COL_AMBER);
    dirty_ = false;
    return;
  }
  const int total = (int)shown_n_ + 1;
  clampScroll(total);
  const int window = total > 4 ? 3 : total;
  for (int i = 0; i < window; ++i) {
    const int index = scroll_ + i;
    if (index >= total) {
      break;
    }
    if (index == (int)shown_n_) {
      drawRow(i, "Обновить", COL_AMBER);
    } else {
      const bool signal = strncmp(shown_[index].name, "Signal", 6) == 0;
      const uint16_t color = signal ? COL_CYAN : COL_WHITE;
      drawRow(i, shown_[index].name, color);
    }
  }
  if (total > 4) {
    drawRow(3, scroll_ + 3 < total ? "дальше" : "в начало", COL_MUTED);
  }
  dirty_ = false;
}

void Ui::paintParam() {
  if (!dirty_) {
    return;
  }
  lcd_->fillScreen(COL_BG);
  drawTextCentered(*lcd_, 120, 26, step_ == 1 ? "Параметр 1" : "Параметр 2", font16, COL_WHITE,
                   COL_BG);
  const int total = PARAM_COUNT + 1;
  clampScroll(total);
  const int window = 3;
  for (int i = 0; i < window; ++i) {
    const int index = scroll_ + i;
    if (index >= total) {
      break;
    }
    if (index == PARAM_COUNT) {
      drawRow(i, "Назад", COL_AMBER);
      continue;
    }
    const ParamDef* param = paramAt((uint8_t)index);
    const bool used = step_ == 2 && param->id == p1_;
    drawRow(i, param->title, used ? COL_TRACK : COL_WHITE);
  }
  drawRow(3, scroll_ + 3 < total ? "дальше" : "в начало", COL_MUTED);
  dirty_ = false;
}

void Ui::paintDash(const SettingsStore& store, const SignalClient& client) {
  const uint8_t id1 = store.ready() ? store.get().param1 : p1_;
  const uint8_t id2 = store.ready() ? store.get().param2 : p2_;
  const Reading a = readingFor(id1, client);
  const Reading b = readingFor(id2, client);
  const Link link = client.link();

  uint16_t rim = COL_RED;
  const char* banner = "нет связи";
  switch (link) {
    case Link::Online:
      rim = client.fresh() ? COL_GREEN : COL_AMBER;
      banner = a.known ? nullptr : "ждём";
      break;
    case Link::Connecting:
    case Link::Scanning:
    case Link::Idle:
      rim = COL_BLUE;
      banner = "подключение";
      break;
    case Link::Down:
      rim = COL_RED;
      banner = "нет связи";
      break;
  }

  const bool toast_done = toast_ != nullptr && millis() >= toast_until_;
  const bool changed = dirty_ || toast_done || a.known != last_ka_ || b.known != last_kb_ ||
                       a.value != last_a_ || b.value != last_b_ || (uint8_t)link != last_link_ ||
                       rim != last_rim_;
  if (!changed) {
    return;
  }

  const bool redraw_all = dirty_ || toast_done;
  drawFace(*lcd_, theme_, a, b, banner, rim, redraw_all);
  last_a_ = a.value;
  last_b_ = b.value;
  last_ka_ = a.known;
  last_kb_ = b.known;
  last_link_ = (uint8_t)link;
  last_rim_ = rim;
  dirty_ = false;
  if (toast_done) {
    toast_ = nullptr;
  }
  if (toast_ != nullptr && millis() < toast_until_) {
    lcd_->fillRoundRect(30, 102, 180, 30, 8, COL_ROW);
    drawTextCentered(*lcd_, 120, 117, toast_, font16, COL_WHITE, COL_ROW);
  }
}

void Ui::paint(const SettingsStore& store, const SignalClient& client) {
  switch (screen_) {
    case Screen::Scan:
      paintScan();
      break;
    case Screen::Devices:
      paintDevices(client.scanning());
      break;
    case Screen::Param:
      paintParam();
      break;
    case Screen::Dash:
      paintDash(store, client);
      break;
  }
}

void Ui::loop(ButtonEvent button, TouchEvent touch, float hold, SettingsStore& store,
              SignalClient& client) {
  if (button == ButtonEvent::Long && !reset_armed_) {
    reset_armed_ = true;
    client.requestStop();
    lcd_->fillScreen(COL_BG);
    drawTextCentered(*lcd_, 120, 120, "Сброс", font34, COL_RED, COL_BG);
  }
  if (reset_armed_) {
    if (!client.busy()) {
      finishReset(store, client);
    } else {
      return;
    }
  }

  if (hold > 0.05f) {
    touch.kind = TouchKind::None;
  }

  if (button == ButtonEvent::Short) {
    if (screen_ == Screen::Dash) {
      theme_ = (uint8_t)((theme_ + 1) % THEME_COUNT);
      store.saveTheme(theme_);
      toast_ = themeName(theme_);
      toast_until_ = millis() + 1100;
      dirty_ = true;
    } else {
      goBack();
    }
  }

  if (screen_ == Screen::Scan || screen_ == Screen::Devices) {
    const uint8_t count = client.copyPeers(shown_, 8);
    const uint16_t rev = client.peerRev();
    if (count != shown_n_ || rev != shown_rev_) {
      shown_n_ = count;
      shown_rev_ = rev;
      dirty_ = true;
    }
  }
  if (screen_ == Screen::Scan && millis() - scan_ms_ > 2000) {
    screen_ = Screen::Devices;
    scroll_ = 0;
    dirty_ = true;
  }

  if (screen_ == Screen::Dash && !hinted_) {
    hinted_ = true;
    toast_ = "кнопка - вид";
    toast_until_ = millis() + 2200;
    dirty_ = true;
  }

  handleTouch(touch, store, client);
  paint(store, client);

  if (hold > 0.35f && !reset_armed_) {
    drawHoldRing(*lcd_, hold);
    hold_drawn_ = true;
  } else if (hold_drawn_) {
    hold_drawn_ = false;
    dirty_ = true;
    paint(store, client);
  }
}
