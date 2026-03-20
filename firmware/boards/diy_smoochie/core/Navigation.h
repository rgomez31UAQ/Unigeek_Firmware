#pragma once

#include "core/INavigation.h"
#include "core/IKeyboard.h"

class NavigationImpl : public INavigation
{
public:
  void begin() override {}

  void update() override {
  }

private:
  Direction  _heldDir = DIR_NONE;
};