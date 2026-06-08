//
// Created by L Shaf on 2026-02-19.
//

#pragma once

#include "core/ScreenMirror.h"

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

    // ── Screen-mirror capture taps for *direct* panel draws ──────────────────
    // Sprites mirror via CaptureSprite; these shadow the draw + text-state calls
    // so direct Uni.Lcd.* draws replicate onto the RAM canvas (a no-op when not
    // streaming). Reached because screens call through the IDisplay static type.
    // Replicating onto the canvas — a real GFX surface — captures everything
    // exactly, including scaled/transparent text and filled shapes that bypass
    // the virtual funnels. `using` keeps base overloads visible (no name-hiding).
    using TFT_eSPI::pushImage;
    using TFT_eSPI::drawString;
    using TFT_eSPI::setTextColor;

    void drawPixel(int32_t x, int32_t y, uint32_t color) override {
      TFT_eSPI::drawPixel(x, y, color);
      Mirror.dot((int16_t)x, (int16_t)y, (uint16_t)color);
    }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
      TFT_eSPI::fillRect(x, y, w, h, color);
      Mirror.solidFill((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (uint16_t)color);
    }
    void fillScreen(uint32_t color) {
      TFT_eSPI::fillScreen(color);
      Mirror.clearScreen((uint16_t)color);
    }
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
      TFT_eSPI::drawRect(x, y, w, h, color);
      Mirror.box((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (uint16_t)color);
    }
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
      TFT_eSPI::drawFastHLine(x, y, w, color);
      Mirror.hLine((int16_t)x, (int16_t)y, (int16_t)w, (uint16_t)color);
    }
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
      TFT_eSPI::drawFastVLine(x, y, h, color);
      Mirror.vLine((int16_t)x, (int16_t)y, (int16_t)h, (uint16_t)color);
    }
    // drawLine / drawCircle aren't shadowed: they funnel through the virtual
    // drawPixel above, so they're already replicated onto the canvas. fillCircle
    // / round-rects use the non-virtual drawFastHLine fast path, so they need
    // explicit replication.
    void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
      TFT_eSPI::fillCircle(x, y, r, color);
      Mirror.fillCirc((int16_t)x, (int16_t)y, (int16_t)r, (uint16_t)color);
    }
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
      TFT_eSPI::drawRoundRect(x, y, w, h, r, color);
      Mirror.rRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)r, (uint16_t)color);
    }
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
      TFT_eSPI::fillRoundRect(x, y, w, h, r, color);
      Mirror.fillRRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)r, (uint16_t)color);
    }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* data) {
      TFT_eSPI::pushImage(x, y, w, h, data);
      Mirror.bitmap((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, data);
    }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
      TFT_eSPI::pushImage(x, y, w, h, data);
      Mirror.bitmap((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, data);
    }
    // Text: replicate state + the string onto the canvas (renders any size /
    // transparency correctly). drawString returns the advance width.
    int16_t drawString(const char* s, int32_t x, int32_t y) {
      int16_t r = TFT_eSPI::drawString(s, x, y);
      Mirror.text(s, (int16_t)x, (int16_t)y);
      return r;
    }
    int16_t drawString(const String& s, int32_t x, int32_t y) {
      int16_t r = TFT_eSPI::drawString(s, x, y);
      Mirror.text(s.c_str(), (int16_t)x, (int16_t)y);
      return r;
    }
    void setTextColor(uint16_t c) {
      TFT_eSPI::setTextColor(c);
      Mirror.tColor(c);
    }
    void setTextColor(uint16_t c, uint16_t bg, bool bgfill = false) {
      TFT_eSPI::setTextColor(c, bg, bgfill);
      Mirror.tColor(c, bg);
    }
    void setTextSize(uint8_t s) {
      TFT_eSPI::setTextSize(s);
      Mirror.tSize(s);
    }
    void setTextDatum(uint8_t d) {
      TFT_eSPI::setTextDatum(d);
      Mirror.tDatum(d);
    }
    void setTextFont(uint8_t f) {
      TFT_eSPI::setTextFont(f);
      Mirror.tFont(f);
    }
  };

  // Drop-in replacement for TFT_eSprite that also mirrors every pushSprite()
  // into ScreenMirror for screen streaming. The codebase uses the concrete
  // `Sprite` alias everywhere, so this captures all blits with no screen edits.
  // When the mirror is inactive each override is one bool check. Method bodies
  // live in core/ScreenMirror.cpp to avoid an include cycle. `using` keeps the
  // base's other pushSprite overloads visible (else they'd be name-hidden).
  class CaptureSprite : public TFT_eSprite
  {
  public:
    CaptureSprite(TFT_eSPI* tft) : TFT_eSprite(tft) {}
    using TFT_eSprite::pushSprite;
    void pushSprite(int32_t x, int32_t y);
    void pushSprite(int32_t x, int32_t y, uint16_t transparent);
  };
  using Sprite = CaptureSprite;
#endif