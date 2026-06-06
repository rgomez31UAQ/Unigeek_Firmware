#pragma once

// Streams the screen to a host by mirroring every draw into a RAM canvas, then
// flushing only the changed (dirty) region — so it works even on write-only
// panels with no MISO readback (Cardputer, sticks3; see memory
// "panel readback impossible").
//
// The canvas is a full-screen TFT_eSprite allocated on START (PSRAM-preferred —
// TFT_eSprite uses PSRAM automatically when present, else internal SRAM) and
// freed on STOP, so a disabled mirror costs zero RAM. Because the canvas is a
// real GFX surface, replicating each draw onto it captures EVERYTHING exactly —
// scaled/transparent text, circles, sprites — closing the gaps the old
// per-op tap-streaming had.
//
// Streaming is event-driven, not timed: draw taps accumulate a dirty rectangle;
// pump() (called once per main-loop iteration) flushes that region and clears
// it. When nothing changed, pump() is a no-op. Emission happens on the main
// task only (the draw taps never touch serial), so the Lua interpreter task
// can draw freely without racing the wire.

#include <Arduino.h>

namespace ScreenProto {
  static constexpr uint8_t CTX     = 'S';
  static constexpr uint8_t C_START = 0x01; // [fps:1] optional (ignored — event-driven)
  static constexpr uint8_t C_STOP  = 0x02;
  static constexpr uint8_t C_INPUT = 0x10; // [dir:1] nav injection
  static constexpr uint8_t T_HELLO = 0xA0; // [w:2][h:2][format:1][rot:1]
  static constexpr uint8_t T_FRAME = 0xA1; // [x:2][y:2][w:2][h:2][rgb565…]
}

class ScreenMirror {
public:
  using Sink = void (*)(void* ctx, uint8_t type, const uint8_t* data, uint32_t len);

  // Master gate from APP_CONFIG_SCREEN_MIRROR. Off ⇒ start() refuses and every
  // tap exits on this bool — no canvas, no work.
  void setEnabled(bool e) { _enabled = e; }
  bool enabled() const { return _enabled; }
  bool active()  const { return _canvas != nullptr; }
  // True when a draw has dirtied the canvas since the last pump() — i.e. a
  // render actually happened. Lets blocking loops pump only on render instead
  // of polling every iteration.
  bool dirty()   const { return _canvas != nullptr && _dx0 <= _dx1; }

  bool start(uint16_t w, uint16_t h, uint32_t maxPayload, Sink sink, void* ctx);
  void stop();
  void pump(); // flush dirty region — call once per main-loop iteration

  uint16_t width()  const { return _w; }
  uint16_t height() const { return _h; }
  void*    canvasRaw() const { return _canvas; } // TFT_eSprite* — for CaptureSprite

  // ── Draw taps (called by IDisplay after the real panel draw) ──────────────
  void solidFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void clearScreen(uint16_t color);
  void box(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void hLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void vLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void dot(int16_t x, int16_t y, uint16_t color);
  void bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* src);
  void text(const char* s, int16_t x, int16_t y);
  void fillCirc(int16_t x, int16_t y, int16_t r, uint16_t color);
  void circ(int16_t x, int16_t y, int16_t r, uint16_t color);
  void fillRRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void rRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void tColor(uint16_t c);
  void tColor(uint16_t c, uint16_t bg);
  void tSize(uint8_t s);
  void tDatum(uint8_t d);
  void tFont(uint8_t f);

  // Expand the dirty rectangle (clipped). Public so CaptureSprite can mark its
  // pushed region after copying the sprite onto the canvas.
  void markDirty(int16_t x, int16_t y, int16_t w, int16_t h);

private:
  bool      _enabled  = false;
  Sink      _sink     = nullptr;
  void*     _ctx      = nullptr;
  void*     _canvas   = nullptr; // TFT_eSprite* (TFT backend); null = inactive
  uint8_t*  _band     = nullptr; // flush staging: [8-byte rect header][pixels]
  uint16_t  _w        = 0;
  uint16_t  _h        = 0;
  uint16_t  _bandRows = 0;

  // Dirty bounding box (inclusive). Empty when _dx0 > _dx1.
  int16_t _dx0 = 0, _dy0 = 0, _dx1 = -1, _dy1 = -1;

  void _emit(uint8_t type, const uint8_t* data, uint32_t len) {
    if (_sink) _sink(_ctx, type, data, len);
  }
};

extern ScreenMirror Mirror;
