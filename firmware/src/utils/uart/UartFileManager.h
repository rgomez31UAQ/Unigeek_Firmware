#pragma once

// UART transport for FileManagerCore. Optional background service — enabled in
// setup() (gated on APP_CONFIG_SERIAL_FM), polled in loop(). The protocol
// details live in FileManagerCore; this class just shuttles bytes between
// Serial and the core's state machine.
//
// The core is heap-allocated so a disabled service costs zero SRAM: begin()
// allocates the ~8 KB FileManagerCore, and update() is a no-op until then.
// This matters on no-PSRAM boards where internal SRAM is scarce.

#include "utils/uart/FileManagerCore.h"

class UartFileManager {
public:
  void begin();              // allocate the core + wire the sender (idempotent)
  void update();             // no-op until begin() has run
  bool isActive() const { return _core != nullptr; }

private:
  FileManagerCore* _core = nullptr;
  static void _sendBytes(const uint8_t* data, size_t len);
};

extern UartFileManager UartFM;
