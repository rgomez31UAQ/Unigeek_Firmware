#pragma once

#include "HIDKeyboardUtil.h"
#include <Arduino.h>

class DuckScriptUtil {
public:
  explicit DuckScriptUtil(HIDKeyboardUtil* keyboard) : _keyboard(keyboard) {}

  bool runCommand(const String& line);

private:
  HIDKeyboardUtil* _keyboard;

  uint8_t _charToHID(const char* str);
  void    _holdPress(uint8_t modifier, uint8_t key);
  void    _holdPress2(uint8_t mod1, uint8_t mod2, uint8_t key);
};
