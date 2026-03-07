#include "APMenuScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/ConfigManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "screens/wifi/access_point/APFileManagerScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/ShowQRCodeAction.h"

// ── Lifecycle ──────────────────────────────────────────────────────────────

void APMenuScreen::onInit()
{
  _showMenu();
}

void APMenuScreen::onItemSelected(uint8_t index)
{
  if (_state == STATE_MENU) {
    switch (index) {
      case 0: { // SSID
        String ssid = InputTextAction::popup("SSID", Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT));
        render();
        if (ssid.isEmpty()) {
          ShowStatusAction::show("SSID is required", 1500);
          render();
          return;
        }
        if (ssid.length() > 32) {
          ShowStatusAction::show("SSID too long (max 32)", 1500);
          render();
          return;
        }
        Config.set(APP_CONFIG_WIFI_AP_SSID, ssid);
        Config.save(Uni.Storage);
        _showMenu();
        break;
      }
      case 1: { // Password
        String pwd = InputTextAction::popup("Password", Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT));
        render();
        if (pwd.length() > 0 && pwd.length() < 8) {
          ShowStatusAction::show("Min 8 chars or empty for open", 1500);
          render();
          return;
        }
        if (pwd.length() > 63) {
          ShowStatusAction::show("Password too long (max 63)", 1500);
          render();
          return;
        }
        Config.set(APP_CONFIG_WIFI_AP_PASSWORD, pwd);
        Config.save(Uni.Storage);
        _showMenu();
        break;
      }
      case 2: { // Hidden toggle — update sublabel in-place, no setItems
        _hidden = !_hidden;
        _menuItems[2].sublabel = _hidden ? "Yes" : "No";
        render();
        break;
      }
      case 3: { // Start
        _startAP();
        break;
      }
    }
  } else {
    switch (index) {
      case 0: { // WiFi QR Code
        String ssid = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
        String pwd  = Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT);
        String qr   = "WIFI:T:" + String(pwd.isEmpty() ? "nopass" : "WPA") + ";S:" + ssid;
        if (!pwd.isEmpty()) qr += ";P:" + pwd;
        qr += ";H:" + String(_hidden ? "true" : "false") + ";;";
        ShowQRCodeAction::show("WiFi QR Code", qr.c_str());
        render();
        break;
      }
      case 1: { // Web File Manager
        Screen.setScreen(new APFileManagerScreen());
        break;
      }
      case 2: { // Stop
        _stopAP();
        break;
      }
    }
  }
}

void APMenuScreen::onBack()
{
  if (_state == STATE_RUNNING) _stopAP();
  Screen.setScreen(new WifiMenuScreen());
}

// ── Private ────────────────────────────────────────────────────────────────

void APMenuScreen::_showMenu()
{
  _state       = STATE_MENU;
  _ssidSub     = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
  String pwd   = Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT);
  _passwordSub = pwd.isEmpty() ? "None" : pwd;
  _menuItems[0] = {"SSID",     _ssidSub.c_str()};
  _menuItems[1] = {"Password", _passwordSub.c_str()};
  _menuItems[2] = {"Hidden",   _hidden ? "Yes" : "No"};
  _menuItems[3] = {"Start"};
  setItems(_menuItems, 4);
}

void APMenuScreen::_showRunning()
{
  _state = STATE_RUNNING;
  _ipSub = WiFi.softAPIP().toString();
  _runningItems[0] = {"WiFi QR Code", _ipSub.c_str()};
  _runningItems[1] = {"Web File Manager"};
  _runningItems[2] = {"Stop"};
  setItems(_runningItems, 3);
}

void APMenuScreen::_startAP()
{
  String ssid = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
  String pwd  = Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(
    IPAddress(10, 0, 0, 1),
    IPAddress(10, 0, 0, 1),
    IPAddress(255, 255, 255, 0)
  );
  WiFi.softAP(ssid.c_str(), pwd.c_str(), 1, _hidden);
  ShowStatusAction::show("AP Started", 1500);
  _showRunning();
}

void APMenuScreen::_stopAP()
{
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_STA);
  ShowStatusAction::show("AP Stopped", 1500);
  _showMenu();
}