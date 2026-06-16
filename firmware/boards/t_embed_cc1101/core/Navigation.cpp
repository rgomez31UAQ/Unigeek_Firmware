//
// LilyGO T-Embed CC1101 navigation implementation
//

#include "Navigation.h"
#include "LedRing.h"

static RotaryEncoder* _encoderPtr = nullptr;

void IRAM_ATTR checkPosition() {
  if (_encoderPtr) _encoderPtr->tick();
}

void NavigationImpl::begin() {
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(ENCODER_BK,  INPUT_PULLUP);

  // TWO03 (2 latch points per cycle) is more tolerant of a dropped quadrature
  // edge than FOUR3 (single latch on state 3): a bounced final edge delays the
  // step by half a detent instead of losing the whole click.
  _encoder    = new RotaryEncoder(ENCODER_A, ENCODER_B, RotaryEncoder::LatchMode::TWO03);
  _encoderPtr = _encoder;

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), checkPosition, CHANGE);

  _lastPos = 0;
  _posDiff = 0;
}

void NavigationImpl::update() {
  unsigned long now = millis();

  // ── Encoder position ────────────────────────────────────────────────────
  int newPos = _encoder->getPosition();
  if (newPos != _lastPos) {
    _posDiff += (newPos - _lastPos);
    _lastPos  = newPos;
  }

  // ── Back button debounce (GPIO 6, active LOW) ────────────────────────────
  bool bkRaw = (digitalRead(ENCODER_BK) == LOW);
  if (bkRaw != _bkRaw) { _bkRaw = bkRaw; _bkChangedAt = now; }
  if (now - _bkChangedAt >= BTN_DEBOUNCE_MS) _bkStable = _bkRaw;

  // ── Select button debounce (GPIO 0, active LOW) ──────────────────────────
  bool selRaw = (digitalRead(ENCODER_BTN) == LOW);
  if (selRaw != _selRaw) { _selRaw = selRaw; _selChangedAt = now; }
  if (now - _selChangedAt >= BTN_DEBOUNCE_MS) _selStable = _selRaw;

  // ── Emit direction ───────────────────────────────────────────────────────
  if (_bkStable) {
    updateState(DIR_BACK);
  } else if (_selStable) {
    updateState(DIR_PRESS);
  } else if (_posDiff <= -DETENT_COUNTS) {
    updateState(DIR_DOWN);
#ifdef DEVICE_T_EMBED_CC1101
    LedRing::addEncoderDelta(-1);
#endif
    _posDiff += DETENT_COUNTS;   // carry remainder, don't discard
  } else if (_posDiff >= DETENT_COUNTS) {
    updateState(DIR_UP);
#ifdef DEVICE_T_EMBED_CC1101
    LedRing::addEncoderDelta(+1);
#endif
    _posDiff -= DETENT_COUNTS;   // carry remainder, don't discard
  } else {
    updateState(DIR_NONE);
  }
}
