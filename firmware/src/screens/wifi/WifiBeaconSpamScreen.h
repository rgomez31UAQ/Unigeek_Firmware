#pragma once

#include "ui/templates/ListScreen.h"

class WifiBeaconSpamScreen : public ListScreen
{
public:
  const char* title() override { return "Beacon Spam"; }
  bool inhibitPowerOff() override { return _state == STATE_SPAMMING; }

  ~WifiBeaconSpamScreen() override;

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onUpdate() override;
  void onBack() override;

private:
  enum State { STATE_MENU, STATE_SPAMMING };
  enum Mode  { MODE_DICTIONARY, MODE_RANDOM };

  State _state = STATE_MENU;
  Mode  _mode  = MODE_DICTIONARY;

  class WifiAttackUtil* _attacker = nullptr;

  static constexpr const char* _spinner = "-\\|/";
  int  _spinIdx  = 0;
  int  _ssidIdx  = 0;
  int  _rounds   = 0;
  char _randomSsid[33] = {};

  String _modeSub;

  ListItem _menuItems[2];

  void _refreshMenu();
  void _start();
  void _stop();
  void _broadcastNext();
  void _drawSpamming();
  void _makeRandomSsid();
};