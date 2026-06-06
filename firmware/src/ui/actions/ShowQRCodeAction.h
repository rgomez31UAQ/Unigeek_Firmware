//
// ShowQRCodeAction.h — render a QR code and block until dismissed.
//

#pragma once

#include "core/Device.h"
#include "utils/uart/UartFileManager.h"
#include "ui/components/QRCodeRenderer.h"

class ShowQRCodeAction
{
public:
  // Renders a QR code for `content` with `label` above it.
  // Blocks until BACK/PRESS. DOWN toggles inverted colors.
  // Wipes the area on exit.
  static void show(const char* label, const char* content, bool inverted = false) {
    QRCodeRenderer::draw(label, content, inverted);

    for (;;) {
      Uni.update();
      UartFM.poll(); // read remote input so nav works in this dialog
      if (Mirror.dirty()) Mirror.pump(); // flush only when this overlay redrew
#ifdef DEVICE_HAS_KEYBOARD
      if (Uni.Keyboard && Uni.Keyboard->available()) {
        Uni.Keyboard->getKey();
        break;
      }
#endif
      if (Uni.Nav->wasPressed()) {
        auto dir = Uni.Nav->readDirection();
        if (dir == INavigation::DIR_DOWN || dir == INavigation::DIR_UP) {
          inverted = !inverted;
          QRCodeRenderer::draw(label, content, inverted);
        } else {
          break;
        }
      }
      delay(10);
    }

    QRCodeRenderer::clear();
    Uni.lastActiveMs = millis();
  }
};