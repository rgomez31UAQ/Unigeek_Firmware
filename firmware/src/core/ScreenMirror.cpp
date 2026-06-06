#include "core/ScreenMirror.h"

ScreenMirror Mirror;

#ifndef DISPLAY_BACKEND_M5GFX
// ─────────────────────────────────────────────────────────────────────────────
// TFT_eSPI backend: full RAM-canvas mirror.
#include "core/IDisplay.h"
#include "core/Device.h"
#include <string.h>

#define CV ((TFT_eSprite*)_canvas)

static inline void _wr16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

bool ScreenMirror::start(uint16_t w, uint16_t h, uint32_t maxPayload, Sink sink, void* ctx) {
  stop();
  if (!_enabled || !w || !h || !sink) return false;

  // Full-screen canvas. TFT_eSprite allocates from PSRAM automatically when
  // present (_psram_enable defaults true), else internal SRAM.
  auto* cv = new TFT_eSprite(&Uni.Lcd);
  cv->setColorDepth(16);
  if (!cv->createSprite(w, h)) { delete cv; return false; } // OOM ⇒ no mirror
  cv->fillSprite(TFT_BLACK);

  // Flush band sized so any region (width ≤ w) fits one transport frame.
  uint32_t maxPixels = (maxPayload - 8) / 2;
  uint16_t rows = (uint16_t)(maxPixels / w);
  if (rows == 0) rows = 1;
  if (rows > h)  rows = h;
  _band = (uint8_t*)malloc(8 + (size_t)w * rows * 2);
  if (!_band) { cv->deleteSprite(); delete cv; return false; }

  _canvas = cv;
  _w = w; _h = h; _bandRows = rows;
  _sink = sink; _ctx = ctx;
  _dx0 = 0; _dy0 = 0; _dx1 = -1; _dy1 = -1; // empty dirty
  return true;
}

void ScreenMirror::stop() {
  if (_canvas) { CV->deleteSprite(); delete CV; _canvas = nullptr; }
  if (_band) { free(_band); _band = nullptr; }
  _w = _h = _bandRows = 0;
  _dx0 = 0; _dy0 = 0; _dx1 = -1; _dy1 = -1;
}

void ScreenMirror::markDirty(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!_canvas) return;
  int16_t x1 = x + w - 1, y1 = y + h - 1;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > (int16_t)_w - 1) x1 = (int16_t)_w - 1;
  if (y1 > (int16_t)_h - 1) y1 = (int16_t)_h - 1;
  if (x > x1 || y > y1) return;
  if (_dx0 > _dx1) { _dx0 = x; _dy0 = y; _dx1 = x1; _dy1 = y1; } // was empty
  else {
    if (x  < _dx0) _dx0 = x;
    if (y  < _dy0) _dy0 = y;
    if (x1 > _dx1) _dx1 = x1;
    if (y1 > _dy1) _dy1 = y1;
  }
}

void ScreenMirror::pump() {
  if (!_canvas || _dx0 > _dx1) return; // nothing changed
  // Snapshot + reset back-to-back so draws on another task re-dirty cleanly.
  int16_t x0 = _dx0, y0 = _dy0, x1 = _dx1, y1 = _dy1;
  _dx1 = -1;

  const int16_t w = x1 - x0 + 1;
  for (int16_t j = y0; j <= y1; j += _bandRows) {
    int16_t bh = (j + (int16_t)_bandRows - 1 <= y1) ? (int16_t)_bandRows : (y1 - j + 1);
    uint16_t* px = (uint16_t*)(_band + 8);
    for (int16_t r = 0; r < bh; r++)
      for (int16_t i = 0; i < w; i++)
        *px++ = CV->readPixel(x0 + i, j + r);
    _wr16(_band + 0, (uint16_t)x0);
    _wr16(_band + 2, (uint16_t)j);
    _wr16(_band + 4, (uint16_t)w);
    _wr16(_band + 6, (uint16_t)bh);
    _emit(ScreenProto::T_FRAME, _band, 8 + (uint32_t)w * bh * 2);
  }
}

void ScreenMirror::solidFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (!_canvas) return;
  CV->fillRect(x, y, w, h, c);
  markDirty(x, y, w, h);
}
void ScreenMirror::clearScreen(uint16_t c) {
  if (!_canvas) return;
  CV->fillSprite(c);
  markDirty(0, 0, (int16_t)_w, (int16_t)_h);
}
void ScreenMirror::box(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (!_canvas) return;
  CV->drawRect(x, y, w, h, c);
  markDirty(x, y, w, h);
}
void ScreenMirror::hLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
  if (!_canvas) return;
  CV->drawFastHLine(x, y, w, c);
  markDirty(x, y, w, 1);
}
void ScreenMirror::vLine(int16_t x, int16_t y, int16_t h, uint16_t c) {
  if (!_canvas) return;
  CV->drawFastVLine(x, y, h, c);
  markDirty(x, y, 1, h);
}
void ScreenMirror::dot(int16_t x, int16_t y, uint16_t c) {
  if (!_canvas) return;
  CV->drawPixel(x, y, c);
  markDirty(x, y, 1, 1);
}
void ScreenMirror::bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* src) {
  if (!_canvas || !src) return;
  CV->pushImage(x, y, w, h, (uint16_t*)src);
  markDirty(x, y, w, h);
}
void ScreenMirror::text(const char* s, int16_t x, int16_t y) {
  if (!_canvas || !s) return;
  CV->drawString(s, x, y);
  int16_t w = (int16_t)CV->textWidth(s);
  int16_t h = (int16_t)CV->fontHeight();
  int16_t px = x, py = y;
  uint8_t d = CV->textdatum; // TL0 TC1 TR2 ML3 MC4 MR5 BL6 BC7 BR8 L9 C10 R11
  if (d == 1 || d == 4 || d == 7 || d == 10) px -= w / 2;
  else if (d == 2 || d == 5 || d == 8 || d == 11) px -= w;
  if (d == 3 || d == 4 || d == 5) py -= h / 2;
  else if (d == 6 || d == 7 || d == 8 || d >= 9) py -= h;
  markDirty(px - 1, py - 1, w + 2, h + 2);
}
void ScreenMirror::fillCirc(int16_t x, int16_t y, int16_t r, uint16_t c) {
  if (!_canvas) return;
  CV->fillCircle(x, y, r, c);
  markDirty(x - r, y - r, 2 * r + 1, 2 * r + 1);
}
void ScreenMirror::circ(int16_t x, int16_t y, int16_t r, uint16_t c) {
  if (!_canvas) return;
  CV->drawCircle(x, y, r, c);
  markDirty(x - r, y - r, 2 * r + 1, 2 * r + 1);
}
void ScreenMirror::fillRRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
  if (!_canvas) return;
  CV->fillRoundRect(x, y, w, h, r, c);
  markDirty(x, y, w, h);
}
void ScreenMirror::rRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
  if (!_canvas) return;
  CV->drawRoundRect(x, y, w, h, r, c);
  markDirty(x, y, w, h);
}
void ScreenMirror::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
  if (!_canvas) return;
  CV->drawLine(x0, y0, x1, y1, c);
  int16_t mnx = x0 < x1 ? x0 : x1, mny = y0 < y1 ? y0 : y1;
  markDirty(mnx, mny, (x0 > x1 ? x0 - x1 : x1 - x0) + 1, (y0 > y1 ? y0 - y1 : y1 - y0) + 1);
}
void ScreenMirror::tColor(uint16_t c)              { if (_canvas) CV->setTextColor(c); }
void ScreenMirror::tColor(uint16_t c, uint16_t bg) { if (_canvas) CV->setTextColor(c, bg); }
void ScreenMirror::tSize(uint8_t s)                { if (_canvas) CV->setTextSize(s); }
void ScreenMirror::tDatum(uint8_t d)               { if (_canvas) CV->setTextDatum(d); }
void ScreenMirror::tFont(uint8_t f)                { if (_canvas) CV->setTextFont(f); }

// CaptureSprite (the `Sprite` alias) — copy each pushed sprite onto the canvas
// so its content is captured, then mark the region dirty. Declared in IDisplay.h.
void CaptureSprite::pushSprite(int32_t x, int32_t y) {
  TFT_eSprite::pushSprite(x, y); // real panel
  void* cv = Mirror.canvasRaw();
  if (!cv || !created()) return;
  pushToSprite((TFT_eSprite*)cv, (int32_t)x, (int32_t)y);
  Mirror.markDirty((int16_t)x, (int16_t)y, (int16_t)width(), (int16_t)height());
}
void CaptureSprite::pushSprite(int32_t x, int32_t y, uint16_t transparent) {
  TFT_eSprite::pushSprite(x, y, transparent);
  void* cv = Mirror.canvasRaw();
  if (!cv || !created()) return;
  pushToSprite((TFT_eSprite*)cv, (int32_t)x, (int32_t)y, transparent);
  Mirror.markDirty((int16_t)x, (int16_t)y, (int16_t)width(), (int16_t)height());
}

#else // ───────────────────────────────────────────────────────────────────────
// M5GFX backend: canvas mirror not supported yet — all no-ops.
bool ScreenMirror::start(uint16_t, uint16_t, uint32_t, Sink, void*) { return false; }
void ScreenMirror::stop() {}
void ScreenMirror::pump() {}
void ScreenMirror::markDirty(int16_t, int16_t, int16_t, int16_t) {}
void ScreenMirror::solidFill(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::clearScreen(uint16_t) {}
void ScreenMirror::box(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::hLine(int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::vLine(int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::dot(int16_t, int16_t, uint16_t) {}
void ScreenMirror::bitmap(int16_t, int16_t, int16_t, int16_t, const uint16_t*) {}
void ScreenMirror::text(const char*, int16_t, int16_t) {}
void ScreenMirror::fillCirc(int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::circ(int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::fillRRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::rRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::line(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
void ScreenMirror::tColor(uint16_t) {}
void ScreenMirror::tColor(uint16_t, uint16_t) {}
void ScreenMirror::tSize(uint8_t) {}
void ScreenMirror::tDatum(uint8_t) {}
void ScreenMirror::tFont(uint8_t) {}
#endif
