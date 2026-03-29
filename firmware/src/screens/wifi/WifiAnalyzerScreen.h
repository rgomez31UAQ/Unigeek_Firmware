#pragma once

#include "ui/templates/ListScreen.h"
#include "ui/views/ScrollListView.h"

class WifiAnalyzerScreen : public ListScreen
{
public:
  const char* title() override { return _title; }

  void onInit() override;
  void onUpdate() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  static constexpr int MAX_SCAN = 20;

  enum State { STATE_SCAN, STATE_INFO };
  State _state = STATE_SCAN;

  struct WifiEntry {
    char ssid[33];
    char bssid[18];
    char rssi[20];
    char channel[4];
    char encryption[20];
  };

  char      _title[16]         = "WiFi Analyzer";
  WifiEntry _entries[MAX_SCAN];
  int       _entryCount        = 0;

  ListItem         _scanItems[MAX_SCAN];
  ScrollListView   _scrollView;
  ScrollListView::Row _infoRows[5];

  void _scan();
  void _showScan();
  void _showInfo(int index);
};