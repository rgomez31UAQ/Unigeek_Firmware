//
// M5StickS3 — NS4168 I2S amp with PM1-controlled PA.
// The speaker amp is gated by PM1 GPIO3 (register 0x11, bit 3).
// Requires MCLK on GPIO 18. Extends SpeakerI2S.
//

#pragma once

#include "core/SpeakerI2S.h"
#include <driver/i2s.h>
#include <Wire.h>

class SpeakerStickS3 : public SpeakerI2S
{
public:
  void begin() override
  {
    // 1. Enable PA: set PM1 GPIO3 as output HIGH
    _pm1SetupGpio3();
    _pm1SetGpio3(true);

    // 2. Install I2S driver with MCLK
    SpeakerI2S::begin();
    i2s_pin_config_t pins = {};
    pins.mck_io_num   = SPK_MCLK;
    pins.bck_io_num   = SPK_BCLK;
    pins.ws_io_num    = SPK_WCLK;
    pins.data_out_num = SPK_DOUT;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    i2s_set_pin((i2s_port_t)SPK_I2S_PORT, &pins);
  }

private:
  static constexpr uint8_t PM1_ADDR = 0x6E;

  static void _pm1BitOn(uint8_t reg, uint8_t mask) {
    Wire1.beginTransmission(PM1_ADDR);
    Wire1.write(reg);
    Wire1.endTransmission(false);
    Wire1.requestFrom(PM1_ADDR, (uint8_t)1);
    uint8_t val = Wire1.available() ? Wire1.read() : 0;
    val |= mask;
    Wire1.beginTransmission(PM1_ADDR);
    Wire1.write(reg);
    Wire1.write(val);
    Wire1.endTransmission();
  }

  static void _pm1BitOff(uint8_t reg, uint8_t mask) {
    Wire1.beginTransmission(PM1_ADDR);
    Wire1.write(reg);
    Wire1.endTransmission(false);
    Wire1.requestFrom(PM1_ADDR, (uint8_t)1);
    uint8_t val = Wire1.available() ? Wire1.read() : 0;
    val &= ~mask;
    Wire1.beginTransmission(PM1_ADDR);
    Wire1.write(reg);
    Wire1.write(val);
    Wire1.endTransmission();
  }

  // Configure PM1 GPIO3 as push-pull output (from M5Unified init)
  static void _pm1SetupGpio3() {
    _pm1BitOff(0x16, 1 << 3);  // GPIO3 as GPIO function (not PWM)
    _pm1BitOn (0x10, 1 << 3);  // GPIO3 mode: output
    _pm1BitOff(0x13, 1 << 3);  // GPIO3 push-pull
  }

  // Set PM1 GPIO3 HIGH (PA enable) or LOW (PA disable)
  static void _pm1SetGpio3(bool high) {
    if (high) _pm1BitOn (0x11, 1 << 3);
    else      _pm1BitOff(0x11, 1 << 3);
  }
};
