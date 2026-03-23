//
// M5StickC Plus 2 — ADC battery on GPIO 38, power hold on GPIO 4.
// No AXP192; uses direct GPIO + deep sleep for power off.
//

#pragma once
#include "core/IPower.h"
#include <driver/gpio.h>
#include <esp_sleep.h>

class PowerImpl : public IPower
{
public:
  void begin() override
  {
    // Keep device alive when USB disconnected
    pinMode(PWR_HOLD_PIN, OUTPUT);
    digitalWrite(PWR_HOLD_PIN, HIGH);

    // Battery ADC
    pinMode(BAT_ADC_PIN, INPUT);
    analogReadMilliVolts(BAT_ADC_PIN);  // warm up ADC calibration
  }

  uint8_t getBatteryPercentage() override
  {
    float mv = (float)analogReadMilliVolts(BAT_ADC_PIN) * 2.0f; // voltage divider x2
    float pct = (mv - 3300.0f) / (4150.0f - 3300.0f) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return (uint8_t)pct;
  }

  void powerOff() override
  {
    digitalWrite(PWR_HOLD_PIN, LOW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_UP, LOW);
    esp_deep_sleep_start();
  }

  bool isCharging() override
  {
    // No charger IC detection on Plus 2
    return false;
  }
};
