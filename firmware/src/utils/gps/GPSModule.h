//
// Created by L Shaf on 2026-03-26.
//

#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include <array>
#include <freertos/task.h>
#include "core/IStorage.h"

class GPSModule
{
public:
  TinyGPSPlus gps;

  enum WardriveState {
    WARDRIVE_IDLE,
    WARDRIVE_SCANNING,
    WARDRIVE_SAVING
  };

  struct FoundEntry {
    char name[33];
    char addr[18];
    int8_t rssi;
    uint8_t channel;
    bool isBle;
  };

  void begin(uint8_t uartNum, uint32_t baudRate, int8_t rxPin, int8_t txPin = -1);
  void end();
  void update();

  String getCurrentDate();
  String getCurrentTime();

  bool initWardrive(IStorage* storage);
  void doWardrive(IStorage* storage);
  void endWardrive();

  WardriveState wardriveStatus() { return _wardriveState; }
  const String& wardriveFilename() { return _filename; }
  const String& wardriveError() { return _lastError; }
  uint16_t discoveredCount() { return _totalDiscovered; }
  uint16_t bleDiscoveredCount() { return _totalBleDiscovered; }
  float totalDistance();
  unsigned long wardriveRuntime();

  uint8_t getRecentFinds(FoundEntry* out, uint8_t maxCount);

private:
  HardwareSerial* _serial = nullptr;
  bool _ownSerial = false;
  WardriveState _wardriveState = WARDRIVE_IDLE;

  String _savePath = "/unigeek/gps/wardriver";
  String _filename;
  String _lastError;
  uint16_t _totalDiscovered = 0;
  uint16_t _totalBleDiscovered = 0;
  unsigned long _lastWifiScan = 0;
  unsigned long _wardriveStartTime = 0;

  void _addWigleRecord(IStorage* storage, const String& ssid, const String& bssid,
                        const String& authMode, int32_t rssi, int32_t channel,
                        const char* type = "WIFI");

  static String _pad(int val, int width);

  // circular dedup buffers (separate for WiFi and BLE)
  using ADDR6 = std::array<uint8_t, 6>;
  static const uint8_t DEDUP_BUF_SIZE = 60;

  ADDR6 _wifiBuf[DEDUP_BUF_SIZE];
  uint8_t _wifiCount = 0;
  uint8_t _wifiHead = 0;
  bool _isWifiScanned(const uint8_t* bssid);

  ADDR6 _bleBuf[DEDUP_BUF_SIZE];
  uint8_t _bleCount = 0;
  uint8_t _bleHead = 0;
  bool _isBleScanned(const uint8_t* addr);

  // Promiscuous mode channel hopping
  uint8_t _currentChannel = 1;
  unsigned long _lastChannelHop = 0;
  void _startPromiscuous();
  void _stopPromiscuous();

  // BLE scanning task
  TaskHandle_t _bleTask = nullptr;
  void _startBleScan();
  void _stopBleScan();

  // Recent finds for log display
  static constexpr uint8_t MAX_RECENT = 20;
  FoundEntry _recentFinds[MAX_RECENT];
  uint8_t _recentHead = 0;
  uint8_t _recentCount = 0;
  void _addRecentFind(const char* name, const char* addr, int8_t rssi, uint8_t channel, bool isBle);

  // Distance tracking (start point to current point)
  double _startLat = 0, _startLng = 0;
  bool _hasStartPos = false;
  static double _haversine(double lat1, double lng1, double lat2, double lng2);
};