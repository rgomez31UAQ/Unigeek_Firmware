//
// Created by L Shaf on 2026-02-23.
//

#include "MainMenuScreen.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "screens/ble/BLEMenuScreen.h"
#include "screens/keyboard/KeyboardMenuScreen.h"
#include "screens/game/GameMenuScreen.h"
#include "screens/module/ModuleMenuScreen.h"
#include "screens/utility/UtilityMenuScreen.h"
#include "screens/setting/SettingScreen.h"
#include "screens/CharacterScreen.h"

void MainMenuScreen::onInit() {
  setItems(_items);
}

void MainMenuScreen::onBack() {
  Screen.setScreen(new CharacterScreen());
}

void MainMenuScreen::onItemSelected(uint8_t index) {
  switch (index) {
  case 0: Screen.setScreen(new WifiMenuScreen());      break;
  case 1: Screen.setScreen(new BLEMenuScreen());       break;
  case 2: Screen.setScreen(new KeyboardMenuScreen());  break;
  case 3: Screen.setScreen(new ModuleMenuScreen());    break;
  case 4: Screen.setScreen(new UtilityMenuScreen());   break;
  case 5: Screen.setScreen(new GameMenuScreen());      break;
  case 6: Screen.setScreen(new SettingScreen());       break;
#ifdef APP_MENU_POWER_OFF
  case 7: Uni.Power.powerOff(); break;
#endif
  }
}