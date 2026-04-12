//
// LilyGO T-Embed CC1101 display — ST7789 170x320 landscape, LEDC backlight on GPIO 21
//

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
      ledcSetup(LCD_BL_CH, 1000, 8);
      ledcAttachPin(LCD_BL, LCD_BL_CH);
      _ready = true;
    }

    ledcWrite(LCD_BL_CH, pct == 0 ? 0 : (uint8_t)((uint32_t)pct * 255 / 100));
  }
};
