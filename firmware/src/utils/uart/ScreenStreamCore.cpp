#include "utils/uart/ScreenStreamCore.h"
#include "core/ScreenMirror.h"
#include "core/Device.h"
#include "core/ScreenManager.h"

void ScreenStreamCore::onFrame(uint8_t ctx, uint8_t type, uint8_t seq, uint8_t* payload, uint32_t len) {
  if (ctx != ScreenProto::CTX) return; // another codec owns this context
  switch (type) {
    case ScreenProto::C_START: _start(seq); break;
    case ScreenProto::C_STOP:  Mirror.stop(); sendOk(ScreenProto::CTX, seq); break;
    case ScreenProto::C_INPUT: {
      // payload[0] = INavigation::Direction (1=UP..6=BACK).
      // payload[1] = event: 0/absent = tap, 1 = one-shot long-press (hold).
      // Counts as activity so a remote session doesn't dim into power-save.
      uint8_t d  = (len > 0) ? payload[0] : 0;
      uint8_t ev = (len > 1) ? payload[1] : 0;
      if (Uni.Nav && d >= INavigation::DIR_UP && d <= INavigation::DIR_BACK) {
        if (ev) Uni.Nav->injectHold((INavigation::Direction)d);
        else    Uni.Nav->inject((INavigation::Direction)d);
        Uni.lastActiveMs = millis();
      }
      break;
    }
    case ScreenProto::C_TOUCH: {
      // payload = [x:2][y:2] (LE) — a touch tap at a panel coordinate.
      if (Uni.Nav && len >= 4) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        INavigation::Direction dir = INavigation::DIR_PRESS;
#ifdef DEVICE_HAS_TOUCH_NAV
        // Same zones the touch panel uses (display coords): left ¼ = BACK,
        // right ¾ top/mid/bottom = UP/PRESS/DOWN — so a click navigates like a tap.
        int16_t w = (int16_t)Uni.Lcd.width(), h = (int16_t)Uni.Lcd.height();
        if      (x < w / 4)        dir = INavigation::DIR_BACK;
        else if (y < h / 3)        dir = INavigation::DIR_UP;
        else if (y < (h * 2) / 3)  dir = INavigation::DIR_PRESS;
        else                       dir = INavigation::DIR_DOWN;
#endif
        Uni.Nav->injectTouch(x, y, dir);
        Uni.lastActiveMs = millis();
      }
      break;
    }
    case ScreenProto::C_KEY: {
      // payload[0] = character. Reaches both nav and text input (shared keyboard).
      if (Uni.Keyboard && len > 0) {
        Uni.Keyboard->inject((char)payload[0]);
        Uni.lastActiveMs = millis();
      }
      break;
    }
    default: sendErr(ScreenProto::CTX, seq, "unknown command");
  }
}

void ScreenStreamCore::_start(uint8_t seq) {
  uint16_t w = (uint16_t)Uni.Lcd.width();
  uint16_t h = (uint16_t)Uni.Lcd.height();
  if (!Mirror.start(w, h, kMaxFrame, _sink, this)) {
    sendErr(ScreenProto::CTX, seq, "oom");
    return;
  }
  _seq = seq;

  // HELLO first so the host sizes its canvas before frames arrive.
  uint8_t hello[6];
  hello[0] = (uint8_t)w; hello[1] = (uint8_t)(w >> 8);
  hello[2] = (uint8_t)h; hello[3] = (uint8_t)(h >> 8);
  hello[4] = 0;          // format 0 = RGB565, little-endian
  uint8_t caps = 0;      // report board input capabilities so the host adapts
#ifdef DEVICE_HAS_TOUCH_NAV
  caps |= ScreenProto::CAP_TOUCH;
#endif
  // Runtime check — robust regardless of build-flag propagation; Uni.Keyboard
  // is non-null only on boards with a physical keyboard.
  if (Uni.Keyboard) caps |= ScreenProto::CAP_KEYBOARD;
  hello[5] = caps;
  sendFrame(ScreenProto::CTX, ScreenProto::T_HELLO, _seq, hello, sizeof(hello));

  // Force a full repaint so the mirror streams the current screen at once —
  // otherwise the host stays blank until the next user action redraws.
  if (Screen.current()) Screen.current()->render();
}

void ScreenStreamCore::_sink(void* ctx, uint8_t type, const uint8_t* data, uint32_t len) {
  ScreenStreamCore* self = static_cast<ScreenStreamCore*>(ctx);
  self->sendFrame(ScreenProto::CTX, type, self->_seq, data, len);
}

void ScreenStreamCore::stop() {
  Mirror.stop();
}

void ScreenStreamCore::pump() {
  Mirror.pump(); // emits the dirty region (if any) on the main task
}
