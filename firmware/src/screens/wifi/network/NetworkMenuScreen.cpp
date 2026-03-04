//
// Created by L Shaf on 2026-02-23.
//

#include "NetworkMenuScreen.h"
#include "core/ScreenManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "screens/wifi/network/WorldClockScreen.h"
#include "screens/wifi/network/IPScannerScreen.h"
#include "screens/wifi/network/PortScannerScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/ShowQRCodeAction.h"
#include <WiFi.h>
#include <esp_wifi.h>

NetworkMenuScreen::NetworkMenuScreen() {
  memset(_scanned,      0, sizeof(_scanned));
  memset(_scannedItems, 0, sizeof(_scannedItems));
}

void NetworkMenuScreen::onInit() {
  WiFi.mode(WIFI_MODE_STA);
  if (WiFi.status() == WL_CONNECTED) {
    _showMenu();
  } else {
    _showWifiList();
  }
}

void NetworkMenuScreen::onBack() {
  WiFi.disconnect(true);
  Screen.setScreen(new WifiMenuScreen());
}

void NetworkMenuScreen::onUpdate() {
  if (_state == STATE_INFORMATION) {
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK) { _showMenu(); return; }
#ifndef DEVICE_HAS_KEYBOARD
      if (dir == INavigation::DIR_PRESS) { _showMenu(); return; }
#endif
      _scrollView.onNav(dir);
    }
    return;
  }
  ListScreen::onUpdate();
}

void NetworkMenuScreen::onItemSelected(uint8_t index) {
  if (_state == STATE_SELECT_WIFI) {
    _connectToSelected(index);
  } else if (_state == STATE_MENU) {
    switch (index) {
      case 0: _showInformation(); break;
      case 1: _showWifiQR(); break;
      case 2: Screen.setScreen(new WorldClockScreen()); break;
      case 3: Screen.setScreen(new IPScannerScreen());  break;
      case 4: Screen.setScreen(new PortScannerScreen()); break;
      case 5: /* TODO: WiNetFileManager */    break;
    }
  } else if (_state == STATE_INFORMATION) {
    _showMenu();
  }
}

// ── helpers ────────────────────────────────────────────

String NetworkMenuScreen::_buildPasswordPath(const char* bssid, const char* ssid) {
  String cleanBssid = bssid;
  cleanBssid.replace(":", "");
  return String(_passwordPath) + "/" + cleanBssid + "_" + ssid + ".pass";
}

String NetworkMenuScreen::_readPassword(const char* bssid, const char* ssid) {
  if (!Uni.Storage) return "";
  String path = _buildPasswordPath(bssid, ssid);
  if (!Uni.Storage->exists(path.c_str())) return "";
  return Uni.Storage->readFile(path.c_str());
}

void NetworkMenuScreen::_savePassword(const char* bssid, const char* ssid, const char* password) {
  if (!Uni.Storage) return;
  String path = _buildPasswordPath(bssid, ssid);
  Uni.Storage->makeDir(_passwordPath);
  Uni.Storage->writeFile(path.c_str(), password);
}

// ── states ─────────────────────────────────────────────

void NetworkMenuScreen::_showMenu() {
  _state = STATE_MENU;
  setItems(_menuItems);
}

void NetworkMenuScreen::_showWifiList() {
  _state    = STATE_SELECT_WIFI;
  _scanning = true;
  ShowStatusAction::show("Scanning...", 0);

  WiFi.scanDelete();
  int total = WiFi.scanNetworks();
  if (total > MAX_WIFI) total = MAX_WIFI;
  _scannedCount = total;

  for (int i = 0; i < total; i++) {
    snprintf(_scanned[i].label, sizeof(_scanned[i].label),
             "[%d] %s", WiFi.RSSI(i), WiFi.SSID(i).c_str());
    strncpy(_scanned[i].bssid, WiFi.BSSIDstr(i).c_str(), sizeof(_scanned[i].bssid));
    strncpy(_scanned[i].ssid,  WiFi.SSID(i).c_str(),     sizeof(_scanned[i].ssid));
    _scannedItems[i] = { _scanned[i].label };
  }

  _scanning = false;
  setItems(_scannedItems, _scannedCount);
}

void NetworkMenuScreen::_connectToSelected(uint8_t index) {
  if (index >= _scannedCount) return;

  const char* bssid = _scanned[index].bssid;
  const char* ssid  = _scanned[index].ssid;

  String saved = _readPassword(bssid, ssid);
  if (saved.length() > 0 && _connect(bssid, ssid, saved.c_str())) return;

  String password = InputTextAction::popup(ssid);
  if (password.length() == 0) return;

  if (!_connect(bssid, ssid, password.c_str())) {
    ShowStatusAction::show("Connection Failed!", 1500);
  }
}

bool NetworkMenuScreen::_connect(const char* bssid, const char* ssid, const char* password) {
  ShowStatusAction::show(("Connecting to " + String(ssid) + "...").c_str(), 0);

  WiFi.begin(ssid, password);
  uint8_t res = WiFi.waitForConnectResult(10000);

  if (res == WL_CONNECTED) {
    _savePassword(bssid, ssid, password);
    _showMenu();
    return true;
  }

  if (res == WL_CONNECT_FAILED)      ShowStatusAction::show("Connection Failed!",   1500);
  else if (res == WL_NO_SSID_AVAIL)  ShowStatusAction::show("SSID Not Available!",  1500);
  else                               ShowStatusAction::show("Connection Error!",     1500);

  render();
  return false;
}

void NetworkMenuScreen::_showWifiQR() {
  String ssid = WiFi.SSID();
  String pass = WiFi.psk();

  String content = "WIFI:T:";
  content += (pass.length() > 0) ? "WPA" : "nopass";
  content += ";S:";
  content += ssid;
  content += ";P:";
  content += pass;
  content += ";;";

  ShowQRCodeAction::show(ssid.c_str(), content.c_str());
  _showMenu();
}

void NetworkMenuScreen::_showInformation() {
  _state = STATE_INFORMATION;
  setItems(nullptr, 0);

  _infoRows[0]  = {"IP",       WiFi.localIP().toString()};
  _infoRows[1]  = {"DNS",      WiFi.dnsIP().toString()};
  _infoRows[2]  = {"Gateway",  WiFi.gatewayIP().toString()};
  _infoRows[3]  = {"Subnet",   WiFi.subnetMask().toString()};
  _infoRows[4]  = {"Channel",  String(WiFi.channel())};
  _infoRows[5]  = {"SSID",     WiFi.SSID()};
  _infoRows[6]  = {"Password", WiFi.psk()};
  _infoRows[7]  = {"RSSI",     String(WiFi.RSSI()) + " dBm"};
  _infoRows[8]  = {"Hostname", String(WiFi.getHostname())};
  _infoRows[9]  = {"MAC",      WiFi.macAddress()};
  _infoRows[10] = {"BSSID",    WiFi.BSSIDstr()};

  _scrollView.setRows(_infoRows, 11);
  _scrollView.render(bodyX(), bodyY(), bodyW(), bodyH());
}