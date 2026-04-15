//
// Created by L Shaf on 2026-02-19.
//

#pragma once

#ifdef DISPLAY_BACKEND_M5GFX
  #include <M5GFX.h>
  class IDisplay : public lgfx::LGFX_Device
  {
  public:
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void powerOff() { setBrightness(0); }
  };
  using Sprite = LGFX_Sprite;
#else
  #include <TFT_eSPI.h>
  class IDisplay : public TFT_eSPI
  {
  public:
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void powerOff() { setBrightness(0); }
  };
  using Sprite = TFT_eSprite;
#endif