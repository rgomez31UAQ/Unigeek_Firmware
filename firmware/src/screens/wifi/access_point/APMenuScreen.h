#pragma once

#include "ui/templates/ListScreen.h"
#include <WiFi.h>

class APMenuScreen : public ListScreen {
public:
  const char* title()    override { return "Access Point"; }
  bool inhibitPowerOff() override { return _state == STATE_RUNNING; }

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  enum State { STATE_MENU, STATE_RUNNING };
  State _state  = STATE_MENU;
  bool  _hidden = false;

  String _ssidSub;
  String _passwordSub;
  String _ipSub;

  ListItem _menuItems[4];
  ListItem _runningItems[3];

  void _showMenu();
  void _showRunning();
  void _startAP();
  void _stopAP();
};