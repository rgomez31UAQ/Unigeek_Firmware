#include "utils/uart/UartFileManager.h"
#include <Arduino.h>

UartFileManager UartFM;

void UartFileManager::_sendBytes(const uint8_t* data, size_t len) {
  Serial.write(data, len);
}

void UartFileManager::begin(bool fmEnabled, bool mirrorEnabled) {
  if (_started) return;
  _started = true;
  if (fmEnabled) {
    _fm = new FileManagerCore();
    _fm->setSender(_sendBytes);
  }
  if (mirrorEnabled) {
    _scr = new ScreenStreamCore();
    _scr->setSender(_sendBytes);
  }
}

void UartFileManager::poll() {
  if (!_fm && !_scr) return;
  while (Serial.available() > 0) {
    uint8_t b = (uint8_t)Serial.read();
    if (_fm)  _fm->onByte(b);   // ctx 'F'
    if (_scr) _scr->onByte(b);  // ctx 'S' (each ignores the other's frames)
  }
}

void UartFileManager::update() {
  poll();
  if (_fm)  _fm->pump();
  if (_scr) _scr->pump();       // flush mirror dirty region (no-op when clean)
}
