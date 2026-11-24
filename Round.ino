// Круглая приборка для платы ESP32-2424S012 (ESP32-C3, GC9A01 240x240, CST816).
//
// Arduino IDE:
//   Плата: ESP32C3 Dev Module
//   USB CDC On Boot: Enabled
//   Flash Size: 4MB
//   Partition Scheme: Huge APP (3MB)
//   Библиотеки: LovyanGFX 1.3.0, NimBLE-Arduino 2.5.x
//
// Кнопка BOOT (GPIO9, обычно сзади):
//   короткое нажатие на приборке — следующий вид
//   короткое нажатие в настройке — шаг назад
//   удержание около 1.3 с — сброс настроек и новый поиск

#include <Wire.h>

#include "BootButton.h"
#include "LGFX_Round.h"
#include "SettingsStore.h"
#include "SignalClient.h"
#include "TouchPad.h"
#include "Ui.h"

static LGFX lcd;
static TouchPad touch;
static BootButton button;
static SettingsStore settings;
static SignalClient client;
static Ui ui;

void setup() {
  Serial.begin(115200);
  delay(150);
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(210);
  lcd.fillScreen(COL_BG);

  Wire.begin(PIN_TP_SDA, PIN_TP_SCL);
  Wire.setClock(400000);
  touch.begin();
  button.begin();
  settings.begin();
  client.begin();
  ui.begin(lcd, settings.get());
  if (settings.ready()) {
    client.connectSaved(settings.get());
  } else {
    ui.openScan(client);
  }
}

void loop() {
  const ButtonEvent key = button.poll();
  const TouchEvent tap = touch.poll();
  client.poll();
  ui.loop(key, tap, button.holdFraction(), settings, client);
  delay(8);
}
