#include "config.h"

static const ParamDef kParams[] = {
    {0x01, "Скорость", "км/ч", 0, 220, false},
    {0x02, "Бензин", "%", 0, 100, true},
    {0x03, "Обороты", "об/мин", 0, 12000, false},
    {0x04, "Температура", "°C", -20, 130, false},
    {0x05, "Передача", "", 0, 6, false},
    {0x06, "Фары", "", 0, 3, false},
    {0x07, "Левый", "", 0, 1, false},
    {0x08, "Правый", "", 0, 1, false},
};

static const char* kThemes[] = {"Дуга", "Стрелка", "Часы", "Цифры", "Столбики"};

const ParamDef* paramById(uint8_t id) {
  for (uint8_t i = 0; i < PARAM_COUNT; ++i) {
    if (kParams[i].id == id) {
      return &kParams[i];
    }
  }
  return nullptr;
}

const ParamDef* paramAt(uint8_t index) {
  if (index >= PARAM_COUNT) {
    return nullptr;
  }
  return &kParams[index];
}

const char* themeName(uint8_t theme) {
  if (theme >= THEME_COUNT) {
    return kThemes[0];
  }
  return kThemes[theme];
}
