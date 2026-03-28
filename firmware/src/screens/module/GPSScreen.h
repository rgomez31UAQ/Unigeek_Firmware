//
// Created by L Shaf on 2026-03-26.
//

#pragma once

#include "ui/templates/ListScreen.h"
#include "ui/components/ScrollListView.h"
#include "ui/components/LogView.h"
#include "utils/gps/GPSModule.h"

class GPSScreen : public ListScreen
{
public:
  const char* title() override { return "GPS"; }
  bool inhibitPowerOff() override { return _state == STATE_WARDRIVING; }

  void onInit() override;
  void onUpdate() override;
  void onRender() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  enum State {
    STATE_LOADING,
    STATE_MENU,
    STATE_INFO,
    STATE_WARDRIVING,
  } _state = STATE_LOADING;

  GPSModule _gps;
  unsigned long _lastRender = 0;
  unsigned long _initTime = 0;
  bool _infoInitialized = false;

  // GPS pin config (set in onInit)
  int8_t _txPin = -1;
  int8_t _rxPin = -1;
  uint32_t _baudRate = 9600;

  ListItem _menuItems[2] = {
    {"View GPS Info"},
    {"Wardriver"},
  };

  ScrollListView _infoView;
  ScrollListView::Row _infoRows[8];

  void _showMenu();
  void _renderInfo();
  void _renderWardriver();
  static void _wardStatusCb(TFT_eSprite& sp, int barY, int width, void* userData);
  LogView _wardLog;

  void _enableGnssPower();
  void _disableGnssPower();
};
