#pragma once

#include "core/IDisplay.h"
#include "pins_arduino.h"

class DisplayImpl : public IDisplay
{
public:
  void setBrightness(uint8_t pct) override {
    if (pct > 100) pct = 100;

    static bool _ready = false;
    if (!_ready) {
      ledcSetup(LCD_BL_CH, 256, 8);
      ledcAttachPin(LCD_BL, LCD_BL_CH);
      _ready = true;
    }

    uint8_t brightness = (uint8_t)((uint32_t)pct * 255 / 100);
    ledcWrite(LCD_BL_CH, brightness);
  }
};
