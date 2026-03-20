#pragma once

#include <esp_wifi.h>
#include "ui/templates/ListScreen.h"

class DNSServer;
class AsyncWebServer;
class AsyncWebServerRequest;

class WifiKarmaScreen : public ListScreen
{
public:
  const char* title() override { return "Karma Attack"; }
  bool inhibitPowerOff() override { return _state == STATE_RUNNING; }

  ~WifiKarmaScreen() override;

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onUpdate() override;
  void onBack() override;

private:
  enum State { STATE_MENU, STATE_SELECT_PORTAL, STATE_RUNNING };

  State _state = STATE_MENU;

  // Settings
  bool   _saveList       = true;
  String _portalFolder;
  int    _waitConnect    = 15;   // seconds to wait for device to connect
  int    _waitInput      = 120;  // seconds to wait for user input on portal

  // Menu
  ListItem _menuItems[5];
  String   _saveListSub;
  String   _portalSub;
  String   _waitConnectSub;
  String   _waitInputSub;

  // Portal selection
  static constexpr int MAX_PORTALS = 10;
  ListItem _portalItems[MAX_PORTALS];
  String   _portalNames[MAX_PORTALS];
  int      _portalCount = 0;

  // Portal HTML
  String _portalHtml;
  String _successHtml;
  String _portalBasePath;

  // Running state
  DNSServer*      _dns    = nullptr;
  AsyncWebServer* _server = nullptr;

  static constexpr int MAX_LOG = 20;
  char     _logLines[MAX_LOG][60];
  int      _logCount      = 0;

  // Probe sniffing
  static constexpr int MAX_SSIDS = 50;
  char     _ssids[MAX_SSIDS][33];
  int      _ssidCount     = 0;
  char     _blacklist[MAX_SSIDS][33];
  int      _blacklistCount = 0;

  // Current attack state
  int      _currentIdx    = 0;      // index into _ssids being attacked
  bool     _apActive      = false;
  unsigned long _apStartTime  = 0;
  unsigned long _inputStartTime = 0;
  bool     _clientConnected = false;

  // Stats
  int      _capturedCount = 0;  // total SSIDs captured via probes
  int      _visitCount    = 0;
  int      _postCount     = 0;
  unsigned long _lastDraw = 0;

  // Probe sniffer
  static WifiKarmaScreen* _instance;
  static void IRAM_ATTR _promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type);
  void _onProbe(const char* ssid);

  void _showMenu();
  void _selectPortal();
  void _startAttack();
  void _stopAttack();
  void _startSniffing();
  void _stopSniffing();
  void _deployAP(const char* ssid);
  void _teardownAP();
  void _loadPortalHtml();
  void _setupWebServer();
  void _saveCaptured(const String& data);
  void _saveSSIDToFile(const char* ssid);
  bool _isBlacklisted(const char* ssid);
  void _blacklistSSID(const char* ssid);
  void _addLog(const char* msg);
  void _drawLog();
};