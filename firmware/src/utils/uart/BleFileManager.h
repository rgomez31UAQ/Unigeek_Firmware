#pragma once

// BLE transport for FileManagerCore over a Nordic UART Service (NUS).
//
// Lifecycle is screen-driven: BleFileManagerScreen::onInit() calls begin(),
// destructor calls end(). Web Bluetooth on the host side connects to the
// same NUS UUIDs and uses the same binary frame protocol as the UART path.

#include "utils/uart/FileManagerCore.h"

class BleFileManager {
public:
  void begin(const char* deviceName = "UniGeek FM");
  void end();
  void update();                       // drain RX buffer into the core
  bool isAdvertising() const;
  bool isConnected() const;
  bool isActive() const { return _active; }

private:
  FileManagerCore _core;
  bool _active = false;
  static void _sendBytes(const uint8_t* data, size_t len);
};

extern BleFileManager BleFM;
