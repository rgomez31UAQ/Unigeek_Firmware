#pragma once

#include "ui/templates/BaseScreen.h"

class CharacterScreen : public BaseScreen
{
public:
  const char* title() override { return nullptr; }  // full-screen home — no header

  // Exception: skip BaseScreen chrome (header / status bar) for full-screen layout
  void update() override;
  void render() override;

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  unsigned long _lastRefreshMs = 0;

  void _enterMainMenu();
};
