//
// DIY Smoochie — 5 buttons: SEL (GPIO 0), UP (41), DOWN (40), RIGHT (38), LEFT/BACK (39).
// All buttons active LOW (internal pull-up).
// LEFT maps to DIR_BACK; RIGHT maps to DIR_RIGHT.
//

#pragma once

#include "core/INavigation.h"

class NavigationImpl : public INavigation
{
public:
  void begin() override {
    pinMode(BTN_SEL,   INPUT_PULLUP);
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
  }

  void update() override {
    if (digitalRead(BTN_UP)    == LOW) { updateState(DIR_UP);    return; }
    if (digitalRead(BTN_DOWN)  == LOW) { updateState(DIR_DOWN);  return; }
    if (digitalRead(BTN_LEFT)  == LOW) { updateState(DIR_LEFT);  return; }
    if (digitalRead(BTN_RIGHT) == LOW) { updateState(DIR_RIGHT); return; }
    if (digitalRead(BTN_SEL)   == LOW) { updateState(DIR_PRESS); return; }
    updateState(DIR_NONE);
  }
};