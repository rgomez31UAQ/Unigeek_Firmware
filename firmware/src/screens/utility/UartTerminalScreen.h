#pragma once
#include "ui/templates/ListScreen.h"
#include "ui/views/LogView.h"
#include <HardwareSerial.h>
#include <FS.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

class UartTerminalScreen : public ListScreen {
public:
  const char* title() override;
  ~UartTerminalScreen();

  void onInit()                      override;
  void onUpdate()                    override;
  void onRender()                    override;
  void onBack()                      override;
  void onItemSelected(uint8_t index) override;

private:
  enum State    { STATE_CONFIG, STATE_RUNNING };
  enum SaveMode { SAVE_NO, SAVE_FILE, SAVE_STREAM_AP, SAVE_STREAM_NET };

  State    _state    = STATE_CONFIG;
  SaveMode _saveMode = SAVE_NO;

  // ── config values ─────────────────────────────────────────
  int    _baud    = 115200;
  int    _rxPin   = -1;
  int    _txPin   = -1;
  String _saveFilename;
  String _apName      = "UartStream";
  String _networkSSID;
  String _networkBssid;

  // ── config list sublabel buffers ──────────────────────────
  char _baudLabel[8]      = {};
  char _rxLabel[6]        = {};
  char _txLabel[6]        = {};
  char _saveModeLabel[16] = {};
  char _saveLabel[32]     = {};
  ListItem _configItems[6];

  // ── terminal state ────────────────────────────────────────
  HardwareSerial    _serial{ 2 };
  bool              _serialRunning = false;
  bool              _hexMode       = false;

  TaskHandle_t      _taskHandle  = nullptr;
  SemaphoreHandle_t _mutex       = nullptr;
  String            _rxSharedBuf;

  LogView  _log;
  String   _rxLineBuf;
  fs::File _logFile;
  uint32_t _lastDrawMs = 0;

  // ── WiFi stream state ─────────────────────────────────────
  static constexpr int MAX_TCP_CLIENTS = 4;
  static constexpr int TCP_PORT        = 23;
  WiFiServer _tcpServer{TCP_PORT};
  WiFiClient _tcpClients[MAX_TCP_CLIENTS];
  bool       _wifiStarted = false;
  bool       _apStarted   = false;

  static void _statusBarCb(Sprite& sp, int barY, int w, void* user);
  static void _serialTask(void* arg);

  void _updateLabels();
  void _rebuildItems();
  void _configBaud();
  void _configRx();
  void _configTx();
  void _configSaveMode();
  void _configSaveFile();
  void _configApName();
  void _configNetwork();
  void _startTerminal();
  void _sendCommand();
  void _drainShared();
  void _openLog();
  void _closeLog();
  void _startWifiStream();
  void _stopWifiStream();
  void _acceptClients();
  void _broadcastLine(const char* line);
};