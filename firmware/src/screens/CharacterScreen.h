#pragma once

#include "ui/templates/BaseScreen.h"

class TFT_eSprite;

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

  ~CharacterScreen() override;

private:
  TFT_eSprite*  _sprite        = nullptr;  // persistent full-screen sprite (PSRAM on BOARD_HAS_PSRAM)
  unsigned long _lastRefreshMs = 0;

  // idle animation
  uint8_t       _animFrame   = 0;       // 0=normal, 1=blink
  unsigned long _lastAnimMs  = 0;

  // dialog bubble  (0=typing  1=pausing — no delete phase)
  uint8_t       _wordIdx     = 0;
  uint8_t       _wordPos     = 0;
  uint8_t       _wordState   = 0;       // 0=typing  1=pausing
  unsigned long _lastCharMs  = 0;
  char          _history[2][20]  = {};  // [0]=oldest  [1]=most-recent completed word

  void _enterMainMenu();
};
