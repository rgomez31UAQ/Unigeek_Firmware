#include "APFileManagerScreen.h"
#include "core/ScreenManager.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "screens/wifi/access_point/APMenuScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"
#include <WiFi.h>

void APFileManagerScreen::onInit()
{
  if (!Uni.Storage || !Uni.Storage->isAvailable()) {
    ShowStatusAction::show("No storage available");
    Screen.setScreen(new APMenuScreen());
    return;
  }
  _showMenu();
}

void APFileManagerScreen::onBack()
{
  if (_state == STATE_RUNNING) {
    _stop();
  } else {
    Screen.setScreen(new APMenuScreen());
  }
}

void APFileManagerScreen::onItemSelected(uint8_t index)
{
  switch (index) {
    case 0: {
      String pw = InputTextAction::popup("Web Password",
        Config.get(APP_CONFIG_WEB_PASSWORD, APP_CONFIG_WEB_PASSWORD_DEFAULT).c_str());
      if (pw.length() > 0) {
        Config.set(APP_CONFIG_WEB_PASSWORD, pw);
        Config.save(Uni.Storage);
      }
      _showMenu();
      break;
    }
    case 1:
      _start();
      break;
  }
}

// ── Private ────────────────────────────────────────────────────────────────

void APFileManagerScreen::_showMenu()
{
  _state       = STATE_MENU;
  _passwordSub = Config.get(APP_CONFIG_WEB_PASSWORD, APP_CONFIG_WEB_PASSWORD_DEFAULT);
  _menuItems[0] = {"Password", _passwordSub.c_str()};
  _menuItems[1] = {"Start"};
  setItems(_menuItems, 2);
}

void APFileManagerScreen::_start()
{
  // Require web page files to be present — direct user to download via Network menu
  const String base = WebFileManager::WEB_PATH;
  bool filesExist = Uni.Storage && Uni.Storage->isAvailable() &&
                    Uni.Storage->exists((base + "/index.htm").c_str());
  if (!filesExist) {
    ShowStatusAction::show("Web page not installed.\nGo to: Network >\nWiFi File Manager\n> Download");
    render();
    return;
  }

  ShowStatusAction::show("Starting server...", 0);
  if (!_server.begin()) {
    ShowStatusAction::show(_server.getError().c_str());
    _showMenu();
    return;
  }
  _state = STATE_RUNNING;
  setItems(nullptr, 0);

  String ipUrl   = "http://" + WiFi.softAPIP().toString() + "/";
  String mdnsUrl = "http://unigeek.local/";
  int cx   = bodyX() + bodyW() / 2;
  int midY = Uni.Lcd.height() / 2;

  Uni.Lcd.setTextFont(1);
  Uni.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  Uni.Lcd.drawCentreString(ipUrl,   cx, midY - 10, 1);
  Uni.Lcd.drawCentreString(mdnsUrl, cx, midY + 4,  1);

  Uni.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  Uni.Lcd.drawCentreString("BACK to stop", cx, bodyY() + bodyH() - 14, 1);
}

void APFileManagerScreen::_stop()
{
  _server.end();
  _showMenu();
}