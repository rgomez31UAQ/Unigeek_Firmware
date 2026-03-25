#include "screens/setting/PinSettingScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/PinConfigManager.h"
#include "screens/setting/SettingScreen.h"
#include "ui/actions/InputNumberAction.h"

void PinSettingScreen::onInit() {
  setItems(_items);
  _refresh();
}

void PinSettingScreen::_refresh() {
  _sdaSub = PinConfig.get(PIN_CONFIG_EXT_SDA, PIN_CONFIG_EXT_SDA_DEFAULT);
  _sclSub = PinConfig.get(PIN_CONFIG_EXT_SCL, PIN_CONFIG_EXT_SCL_DEFAULT);

  _items[0].sublabel = _sdaSub.c_str();
  _items[1].sublabel = _sclSub.c_str();

  render();
}

void PinSettingScreen::onItemSelected(uint8_t index) {
  switch (index) {
    case 0: {
      int cur = PinConfig.getInt(PIN_CONFIG_EXT_SDA, PIN_CONFIG_EXT_SDA_DEFAULT);
      int val = InputNumberAction::popup("External SDA Pin", 0, 48, cur);
      PinConfig.set(PIN_CONFIG_EXT_SDA, String(val));
      PinConfig.save(Uni.Storage);
      _refresh();
      break;
    }
    case 1: {
      int cur = PinConfig.getInt(PIN_CONFIG_EXT_SCL, PIN_CONFIG_EXT_SCL_DEFAULT);
      int val = InputNumberAction::popup("External SCL Pin", 0, 48, cur);
      PinConfig.set(PIN_CONFIG_EXT_SCL, String(val));
      PinConfig.save(Uni.Storage);
      _refresh();
      break;
    }
  }
}

void PinSettingScreen::onBack() {
  Screen.setScreen(new SettingScreen());
}