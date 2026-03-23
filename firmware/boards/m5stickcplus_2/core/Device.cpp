//
// M5StickC Plus 2 — ESP32 with 8MB flash, PSRAM.
// No AXP192; uses GPIO 4 power hold, ADC battery, PWM backlight.
//

#include "core/Device.h"
#include "core/StorageLFS.h"
#include "core/ConfigManager.h"
#include "Navigation.h"
#include "EncoderNavigation.h"
#include "Display.h"
#include "Power.h"
#include "Speaker.h"
#include <Wire.h>

static DisplayImpl          display;
static NavigationImpl       navigation;
static EncoderNavigation    encoderNavigation;
static PowerImpl            power;
static StorageLFS        storageLFS;
static SpeakerBuzzer     speaker;

Device* Device::createInstance() {
  pinMode(BTN_UP, INPUT);
  pinMode(BTN_A,  INPUT);
  pinMode(BTN_B,  INPUT);

  // Power hold — keep device alive when USB disconnected
  pinMode(PWR_HOLD_PIN, OUTPUT);
  digitalWrite(PWR_HOLD_PIN, HIGH);

  // Internal I2C for BM8563 RTC
  Wire1.begin(INTERNAL_SDA, INTERNAL_SCL);

  // PWM backlight
  display.initBacklight();

  storageLFS.begin();

  return new Device(display, power, &navigation, nullptr,
                    nullptr, &storageLFS, nullptr, &speaker);
}

void Device::applyNavMode() {
  String mode = Config.get(APP_CONFIG_NAV_MODE, APP_CONFIG_NAV_MODE_DEFAULT);
  if (mode == "encoder") {
    switchNavigation(&encoderNavigation);
  } else {
    switchNavigation(&navigation);
  }
}

void Device::boardHook() {
  // 3s hold on BTN_A resets nav mode to default
  static unsigned long _btnAHeld = 0;
  if (digitalRead(BTN_A) == LOW) {
    if (_btnAHeld == 0) _btnAHeld = millis();
    else if (millis() - _btnAHeld >= 3000) {
      Config.set(APP_CONFIG_NAV_MODE, APP_CONFIG_NAV_MODE_DEFAULT);
      Config.save(Storage);
      applyNavMode();
      _btnAHeld = 0;
    }
  } else {
    _btnAHeld = 0;
  }
}
