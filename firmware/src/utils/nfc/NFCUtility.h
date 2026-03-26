//
// Created by L Shaf on 2026-03-25.
//

#pragma once

#include <Arduino.h>
#include <array>

class NFCUtility
{
public:
  using mfKey = std::array<uint8_t, 6>;

  struct MIFARE_Key {
    bool has_value = false;
    mfKey key{};

    MIFARE_Key() = default;
    explicit MIFARE_Key(const mfKey& k) : has_value(true), key(k) {}
    MIFARE_Key(byte i0, byte i1, byte i2, byte i3, byte i4, byte i5) {
      key = {i0, i1, i2, i3, i4, i5};
      has_value = true;
    }

    void reset() { has_value = false; key = {}; }
    void set(const mfKey& k) { key = k; has_value = true; }

    bool has() const { return has_value; }
    explicit operator bool() const { return has_value; }

    const mfKey& value() const { return key; }
    mfKey& value() { return key; }

    std::string c_str() const {
      if (!has_value) return "??:??:??:??:??:??";
      char buffer[20];
      sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
        key[0], key[1], key[2], key[3], key[4], key[5]);
      return buffer;
    }
  };

  static std::array<MIFARE_Key, 9> getDefaultKeys() {
    return {
      MIFARE_Key{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
      MIFARE_Key{ 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5 },
      MIFARE_Key{ 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5 },
      MIFARE_Key{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
      MIFARE_Key{ 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
      MIFARE_Key{ 0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD },
      MIFARE_Key{ 0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A },
      MIFARE_Key{ 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 },
      MIFARE_Key{ 0x71, 0x4C, 0x5C, 0x88, 0x05, 0x92 }
    };
  }
};