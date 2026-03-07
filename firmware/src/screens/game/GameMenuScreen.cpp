#include "GameMenuScreen.h"
#include "core/ScreenManager.h"
#include "screens/MainMenuScreen.h"
#include "screens/game/GameDecoderScreen.h"

void GameMenuScreen::onInit()
{
  setItems(_items);
}

void GameMenuScreen::onBack()
{
  Screen.setScreen(new MainMenuScreen());
}

void GameMenuScreen::onItemSelected(uint8_t index)
{
  switch (index) {
    case 0: Screen.setScreen(new GameDecoderScreen()); break;
  }
}