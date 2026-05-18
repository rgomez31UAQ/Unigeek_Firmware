//
// M5StickC Plus 2 — Encoder navigation with M5Hat Mini EncoderC.
// No AXP192; BTN_UP (GPIO 35) replaces AXP power button for DIR_LEFT.
// BTN_A = back, BTN_UP = left, BTN_B = right, encoder = up/down/press
//

#pragma once

#include "core/INavigation.h"
#include "utils/M5HatMiniEncoderC_Soft.h"

static M5HatMiniEncoderC_Soft encoder;

class EncoderNavigation : public INavigation
{
public:
  void begin() override
  {
    encoder.begin();
  }

  void update() override
  {
    // BTN_A short press (< 3 s) = back; 3 s hold is handled in Device::boardHook()
    bool btnA = (digitalRead(BTN_A) == LOW);
    if (btnA && !_btnAWasLow) {
      _btnAStart  = millis();
      _btnAWasLow = true;
    } else if (!btnA && _btnAWasLow) {
      if (millis() - _btnAStart < 3000) _emitBack = true;
      _btnAWasLow = false;
    }
    if (_emitBack) { _emitBack = false; updateState(DIR_BACK); return; }

    // BTN_UP release = left
    bool btnUp = (digitalRead(BTN_UP) == LOW);
    if (btnUp && !_btnUpWasLow) {
      _btnUpWasLow = true;
    } else if (!btnUp && _btnUpWasLow) {
      _emitLeft    = true;
      _btnUpWasLow = false;
    }
    if (_emitLeft) { _emitLeft = false; updateState(DIR_LEFT); return; }

    // BTN_B release = right
    bool btnB = (digitalRead(BTN_B) == LOW);
    if (btnB && !_btnBWasLow) {
      _btnBWasLow = true;
    } else if (!btnB && _btnBWasLow) {
      _emitRight  = true;
      _btnBWasLow = false;
    }
    if (_emitRight) { _emitRight = false; updateState(DIR_RIGHT); return; }

    // Encoder
    const bool rotLeft  = encoder.getEncoderValue() <= -2;
    const bool rotRight = encoder.getEncoderValue() >= 2;
    if (rotLeft || rotRight) encoder.resetCounter();

    if (!encoder.getButtonStatus()) updateState(DIR_PRESS);
    else if (rotLeft)               updateState(DIR_UP);
    else if (rotRight)              updateState(DIR_DOWN);
    else                            updateState(DIR_NONE);
  }

private:
  unsigned long _btnAStart   = 0;
  bool          _btnAWasLow  = false;
  bool          _emitBack    = false;
  bool          _btnUpWasLow = false;
  bool          _emitLeft    = false;
  bool          _emitRight   = false;
  bool          _btnBWasLow  = false;
};
