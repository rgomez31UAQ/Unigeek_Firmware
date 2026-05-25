#pragma once

// UART transport for FileManagerCore. Always-on background service —
// initialized in setup(), polled in loop(). The protocol details live in
// FileManagerCore; this class just shuttles bytes between Serial and the
// core's state machine.

#include "utils/uart/FileManagerCore.h"

class UartFileManager {
public:
  void begin();
  void update();

private:
  FileManagerCore _core;
  static void _sendBytes(const uint8_t* data, size_t len);
};

extern UartFileManager UartFM;
