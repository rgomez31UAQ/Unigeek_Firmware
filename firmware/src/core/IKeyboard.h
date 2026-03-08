//
// Created by L Shaf on 2026-02-23.
//

#pragma once
#include <stdint.h>

class IKeyboard
{
public:
  enum Modifier : uint8_t {
    MOD_NONE  = 0,
    MOD_SHIFT = 1 << 0,
    MOD_FN    = 1 << 1,
    MOD_CAPS  = 1 << 2,
    MOD_CTRL  = 1 << 3,
    MOD_ALT   = 1 << 4,
    MOD_OPT   = 1 << 5,
  };

  virtual ~IKeyboard() = default;
  virtual void begin()  = 0;
  virtual void update() = 0;
  virtual bool available() = 0;
  virtual char peekKey()   = 0;  // read without consuming
  virtual char getKey()    = 0;
  virtual uint8_t modifiers() { return MOD_NONE; }
  virtual bool isKeyHeld() const { return false; }  // true while the last consumed key is still physically held
};