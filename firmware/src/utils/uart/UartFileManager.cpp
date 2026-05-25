#include "utils/uart/UartFileManager.h"
#include <Arduino.h>

UartFileManager UartFM;

void UartFileManager::_sendBytes(const uint8_t* data, size_t len) {
  Serial.write(data, len);
}

void UartFileManager::begin() {
  if (_core) return;
  _core = new FileManagerCore();
  _core->setSender(_sendBytes);
}

void UartFileManager::update() {
  if (!_core) return;
  while (Serial.available() > 0) {
    _core->onByte((uint8_t)Serial.read());
  }
  _core->pump();
}
