#pragma once

#include "ui/templates/ListScreen.h"
#include "utils/RogueDnsServer.h"
#include "utils/WebFileManager.h"

class WifiAPScreen : public ListScreen {
public:
  const char* title()    override { return "Access Point"; }
  bool inhibitPowerOff() override { return _state != STATE_MENU; }

  void onInit() override;
  void onUpdate() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

  void logVisit(const char* msg);

private:
  enum State { STATE_MENU, STATE_LOG };
  State _state  = STATE_MENU;
  bool  _hidden = false;
  bool  _rogueEnabled = false;
  bool  _captiveEnabled = false;
  bool  _fileManagerEnabled = false;

  String _ssidSub;
  String _passwordSub;
  String _rogueSub;
  String _captiveSub;
  String _fmSub;
  String _captivePath;

  ListItem _menuItems[7];

  RogueDnsServer _rogueServer;
  WebFileManager _fileManager;

  // Log view
  static constexpr int MAX_LOG = 30;
  char _logLines[MAX_LOG][60];
  int  _logCount = 0;
  unsigned long _lastDraw = 0;

  void _showMenu();
  void _showLog();
  void _startAP();
  void _stopAP();
  void _addLog(const char* msg);
  void _drawLog();
};
