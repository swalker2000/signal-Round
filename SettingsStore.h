#pragma once

#include <Preferences.h>

#include "config.h"

class SettingsStore {
 public:
  void begin();
  const SavedLink& get() const { return link_; }
  bool ready() const { return link_.ready; }
  void save(const SavedLink& link);
  void saveTheme(uint8_t theme);
  void clear();

 private:
  void load();

  Preferences prefs_;
  SavedLink link_{};
};
