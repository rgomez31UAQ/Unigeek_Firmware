//
// ShowQRCodeAction.h — render a QR code and block until dismissed.
//

#pragma once

#include "core/Device.h"
#include "ui/components/StatusBar.h"
#include <TFT_eSPI.h>
#include "lgfx_qrcode.h"

class ShowQRCodeAction
{
public:
  // Renders a QR code for `content` with `label` above it.
  // Blocks until any key/nav press. Wipes the area on exit.
  static void show(const char* label, const char* content, bool inverted = false) {
    ShowQRCodeAction action(label, content, inverted);
    action._run();
  }

private:
  const char* _label;
  const char* _content;
  bool        _inverted;

  ShowQRCodeAction(const char* label, const char* content, bool inverted)
    : _label(label), _content(content), _inverted(inverted) {}

  void _run() {
    auto& lcd = Uni.Lcd;

    // Render status bar first — its WIDTH defines the QR area boundary
    StatusBar::refresh();

    const int aX = StatusBar::WIDTH;
    const int aW = lcd.width()  - aX;
    const int aH = lcd.height();
    const int labelH = 14;

    // Auto-find QR version (same approach as puteros/M5GFX Template::renderQRCode)
    for (int version = 1; version <= 40; ++version) {
      QRCode qr;
      auto buf = (uint8_t*)alloca(lgfx_qrcode_getBufferSize(version));
      if (0 != lgfx_qrcode_initText(&qr, buf, version, 0, _content)) continue;

      int ps = min(aW - 4, aH - labelH - 4) / (int)qr.size;
      if (ps < 1) ps = 1;

      int qrW = qr.size * ps;
      int qrH = qr.size * ps;
      int qrX = aX + (aW - qrW) / 2;
      int qrY = labelH + (aH - labelH - qrH) / 2;

      uint16_t bg = _inverted ? TFT_BLACK : TFT_WHITE;
      uint16_t fg = _inverted ? TFT_WHITE : TFT_BLACK;

      lcd.fillRect(aX, 0, aW, aH, bg);
      lcd.setTextColor(fg, bg);
      lcd.setTextDatum(TC_DATUM);
      lcd.setTextSize(1);
      lcd.drawString(_label, aX + aW / 2, 2);

      for (uint8_t y = 0; y < qr.size; y++) {
        for (uint8_t x = 0; x < qr.size; x++) {
          if (lgfx_qrcode_getModule(&qr, x, y)) {
            lcd.fillRect(qrX + x * ps, qrY + y * ps, ps, ps, fg);
          }
        }
      }
      break;
    }

    // Block until any nav/key press
    for (;;) {
      Uni.update();
#ifdef DEVICE_HAS_KEYBOARD
      if (Uni.Keyboard && Uni.Keyboard->available()) {
        Uni.Keyboard->getKey();
        break;
      }
#endif
      if (Uni.Nav->wasPressed()) {
        Uni.Nav->readDirection();
        break;
      }
      delay(10);
    }

    lcd.fillRect(aX, 0, aW, aH, TFT_BLACK);
  }
};