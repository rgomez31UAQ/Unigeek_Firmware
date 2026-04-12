//
// WiFi Marauder v7 power — no battery IC, no charger.
// Battery level is unknown; powerOff uses deep sleep with ext0 wakeup on BTN_SEL (GPIO34).
//

#pragma once

#include "core/IPower.h"
#include "pins_arduino.h"

class PowerImpl : public IPower
{
public:
  void begin() override {}

  // No battery monitoring hardware — return midpoint
  uint8_t getBatteryPercentage() override { return 50; }

  bool isCharging() override { return false; }

  void powerOff() override {
    // GPIO34 is RTC_GPIO4 on ESP32 — valid for ext0 wakeup
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_SEL, LOW);
    esp_deep_sleep_start();
  }
};