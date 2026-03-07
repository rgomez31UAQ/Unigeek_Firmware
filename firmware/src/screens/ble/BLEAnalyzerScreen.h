#pragma once

#include "ui/templates/ListScreen.h"
#include <NimBLEDevice.h>

class BLEAnalyzerScreen : public ListScreen {
public:
  const char* title() override { return "BLE Analyzer"; }

  ~BLEAnalyzerScreen() override;
  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  enum State {
    STATE_SCAN,
    STATE_LIST,
    STATE_INFO,
    STATE_SERVICE_UUID,
    STATE_SERVICE_DATA,
    STATE_MANUFACTURE_DATA,
  } _state = STATE_SCAN;

  static constexpr uint8_t kMaxDevices = 40;
  static constexpr uint8_t kMaxDetail  = 16;

  NimBLEScan*       _bleScan           = nullptr;
  NimBLEScanResults _scanResults;
  int               _selectedDeviceIdx = -1;

  // Device list storage
  String   _devLabel[kMaxDevices];
  String   _devSub[kMaxDevices];
  ListItem _devItems[kMaxDevices];
  uint8_t  _devCount = 0;

  // Info view (9 fixed rows)
  String   _infoVal[9];
  ListItem _infoItems[9];

  // Detail view (UUIDs / data)
  String   _detailLabel[kMaxDetail];
  ListItem _detailItems[kMaxDetail];
  uint8_t  _detailCount = 0;

  void _startScan();
  void _showList();
  void _showInfo();
  void _showDetail(State state);
};