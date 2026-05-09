#pragma once
#include "ui/templates/ListScreen.h"
#include "utils/network/PrinterPrankUtil.h"

class PrinterPrankScreen : public ListScreen
{
public:
  PrinterPrankScreen();

  const char* title()    override { return "Printer Prank"; }
  bool inhibitPowerOff() override {
    return _state == STATE_DISCOVERING || _state == STATE_PRINTING;
  }

  void onInit() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  enum State {
    STATE_CONFIG,
    STATE_DISCOVERING,
    STATE_DEVICES,
    STATE_PRINTING,
  };

  struct MessagePreset {
    const char* label;
    const char* body;
  };

  static const MessagePreset PRESETS[];
  static const uint8_t       PRESET_COUNT;

  State    _state      = STATE_CONFIG;
  uint8_t  _presetIdx  = 0;
  String   _customMsg;            // overrides preset when non-empty
  String   _msgSub;               // sublabel for "Message" item

  PrinterPrankUtil::Device _devices[PrinterPrankUtil::MAX_DEVICES];
  uint8_t                  _devCount = 0;
  ListItem                 _devItems[PrinterPrankUtil::MAX_DEVICES + 1];
  String                   _devSubs[PrinterPrankUtil::MAX_DEVICES];

  ListItem _configItems[3];

  void _showConfig();
  void _showDevices();
  void _discover();
  void _print(uint8_t index);     // index == _devCount means "Print to All"
  void _pickPreset();
  void _setCustom();

  const char* _currentBody()  const;
  const char* _currentLabel() const;
};
