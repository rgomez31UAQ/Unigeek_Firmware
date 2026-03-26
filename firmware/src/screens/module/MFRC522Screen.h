#pragma once

#include <MFRC522_I2C.h>
#include <array>
#include <unordered_map>
#include "ui/templates/ListScreen.h"
#include "ui/components/ScrollListView.h"
#include "utils/NFCUtility.h"

class MFRC522Screen : public ListScreen
{
public:
  const char* title() override;
  bool inhibitPowerSave() override { return true; }

  void onInit() override;
  void onUpdate() override;
  void onRender() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  enum State_e {
    STATE_MAIN_MENU,
    STATE_SCAN_UID,
    STATE_AUTHENTICATE,
    STATE_MIFARE_CLASSIC,
    STATE_SHOW_KEY,
    STATE_MEMORY_READER,
  };

  State_e _state = STATE_MAIN_MENU;
  MFRC522_I2C* _module = nullptr;
  bool _moduleReady = false;
  TwoWire* _activeBus = nullptr;  // which I2C bus is in use (ExI2C or InI2C)

  MFRC522_I2C::Uid _currentCard = {};
  std::array<std::pair<NFCUtility::MIFARE_Key, NFCUtility::MIFARE_Key>, 40> _mf1AuthKeys;
  std::unordered_map<uint8_t, std::pair<size_t, size_t>> _mf1CardDetails = {
    { MFRC522_I2C::PICC_TYPE_MIFARE_MINI, { 5, 20 } },
    { MFRC522_I2C::PICC_TYPE_MIFARE_1K,   { 16, 64 } },
    { MFRC522_I2C::PICC_TYPE_MIFARE_4K,   { 40, 256 } }
  };

  // Main menu
  ListItem _mainItems[2] = {
    {"Scan UID"},
    {"MIFARE Classic"},
  };

  // MIFARE Classic submenu
#ifdef BOARD_HAS_PSRAM
  ListItem _mfItems[3] = {
    {"Discovered Keys"},
    {"Dump Memory"},
    {"Nested Attack"},
  };
#else
  ListItem _mfItems[2] = {
    {"Discovered Keys"},
    {"Dump Memory"},
  };
#endif

  // ScrollListView for keys/memory display
  ScrollListView _scrollView;
  static constexpr size_t MAX_ROWS = 520;
  ScrollListView::Row _rows[MAX_ROWS];
  String _rowLabels[MAX_ROWS];
  String _rowValues[MAX_ROWS];
  uint16_t _rowCount = 0;

  void _initModule();
  void _cleanup();
  void _goMainMenu();
  void _goMifareClassic();
  void _goShowDiscoveredKeys();
  void _callScanUid();
  void _callAuthenticate();
  void _callMemoryReader();
#ifdef BOARD_HAS_PSRAM
  void _callNestedAttack();
#endif
  bool _resetCardState();

  std::string _uidToString(byte* uid, byte len) {
    std::string s;
    for (byte i = 0; i < len; i++) {
      char buf[3];
      sprintf(buf, "%02X", uid[i]);
      s += buf;
    }
    return s;
  }
};