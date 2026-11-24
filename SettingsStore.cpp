#include "SettingsStore.h"

#include <Arduino.h>
#include <cstring>

void SettingsStore::begin() {
  prefs_.begin("round", false);
  load();
}

void SettingsStore::load() {
  memset(&link_, 0, sizeof(link_));
  link_.ready = prefs_.getBool("ok", false);
  prefs_.getString("name", link_.name, sizeof(link_.name));
  prefs_.getString("addr", link_.addr, sizeof(link_.addr));
  link_.addr_type = prefs_.getUChar("ty", 0);
  link_.param1 = prefs_.getUChar("p1", 0x01);
  link_.param2 = prefs_.getUChar("p2", 0x03);
  link_.theme = prefs_.getUChar("th", 0);
  if (link_.theme >= THEME_COUNT) {
    link_.theme = 0;
  }
  if (paramById(link_.param1) == nullptr) {
    link_.param1 = 0x01;
  }
  if (paramById(link_.param2) == nullptr || link_.param2 == link_.param1) {
    link_.param2 = 0x03;
  }
  if (!link_.ready || link_.addr[0] == '\0') {
    link_.ready = false;
  }
}

void SettingsStore::save(const SavedLink& link) {
  link_ = link;
  link_.ready = true;
  link_.name[sizeof(link_.name) - 1] = '\0';
  link_.addr[sizeof(link_.addr) - 1] = '\0';
  if (link_.theme >= THEME_COUNT) {
    link_.theme = 0;
  }
  prefs_.putBool("ok", true);
  prefs_.putString("name", link_.name);
  prefs_.putString("addr", link_.addr);
  prefs_.putUChar("ty", link_.addr_type);
  prefs_.putUChar("p1", link_.param1);
  prefs_.putUChar("p2", link_.param2);
  prefs_.putUChar("th", link_.theme);
}

void SettingsStore::saveTheme(uint8_t theme) {
  if (!link_.ready) {
    return;
  }
  if (theme >= THEME_COUNT) {
    theme = 0;
  }
  link_.theme = theme;
  prefs_.putUChar("th", theme);
}

void SettingsStore::clear() {
  prefs_.clear();
  memset(&link_, 0, sizeof(link_));
}
