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
      ledcSetup(7, 5000, 8);
      ledcAttachPin(LCD_BL, 7);
      _ready = true;
    }

    ledcWrite(7, (uint32_t)pct * 255 / 100);
  }
};