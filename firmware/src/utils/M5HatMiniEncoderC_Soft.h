//
// Software-bit-banged I2C driver for the M5Hat Mini-EncoderC.
//
// Replaces m5stack/M5Hat-Mini-EncoderC for boards (m5stickcplus_11 / _2) where
// the encoder HAT shares Wire with other peripherals — there's no spare
// hardware I2C peripheral on ESP32, so we bit-bang on the HAT pins (SDA=0,
// SCL=26 by default) and leave Wire/Wire1 untouched for Grove ExI2C and the
// internal RTC/AXP bus.
//
// Protocol matches the encoder MCU (I2C addr 0x42):
//   reg 0x00 : encoder counter (int32 LE, 4 bytes)
//   reg 0x20 : button status   (1 byte, 0 = pressed)
//   reg 0x40 : reset counter   (write any byte)
//

#pragma once
#include <Arduino.h>

class M5HatMiniEncoderC_Soft
{
public:
  static constexpr uint8_t kAddr = 0x42;

  bool begin(uint8_t sda = 0, uint8_t scl = 26) {
    _sda = sda;
    _scl = scl;
    pinMode(_sda, INPUT_PULLUP);
    pinMode(_scl, INPUT_PULLUP);
    delay(2);
    // Probe: START → addr-W → STOP, ACK ⇒ present.
    _start();
    bool ok = _writeByte(kAddr << 1);
    _stop();
    return ok;
  }

  int32_t getEncoderValue() {
    uint8_t d[4];
    if (!_readReg(0x00, d, 4)) return 0;
    return (int32_t)((uint32_t)d[0] | ((uint32_t)d[1] << 8) |
                     ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24));
  }

  bool getButtonStatus() {
    uint8_t d = 0xFF;
    if (!_readReg(0x20, &d, 1)) return true;  // safe default = not-pressed
    return d != 0;
  }

  void resetCounter() {
    uint8_t one = 1;
    _writeReg(0x40, &one, 1);
  }

private:
  static constexpr uint32_t kHalfUs = 5;  // ~100 kHz bus

  uint8_t _sda = 0;
  uint8_t _scl = 26;

  // Open-drain emulation: drive LOW by switching to OUTPUT LOW, release HIGH
  // by switching back to INPUT_PULLUP (line floats high via pullups).
  inline void _sdaHi() { pinMode(_sda, INPUT_PULLUP); }
  inline void _sdaLo() { digitalWrite(_sda, LOW); pinMode(_sda, OUTPUT); }
  inline void _sclHi() {
    pinMode(_scl, INPUT_PULLUP);
    // Allow up to ~1 ms of clock stretching from the slave.
    for (uint32_t spin = 1000; spin && !digitalRead(_scl); spin--) delayMicroseconds(1);
  }
  inline void _sclLo() { digitalWrite(_scl, LOW); pinMode(_scl, OUTPUT); }
  inline bool _sdaRead() { return digitalRead(_sda); }

  void _start() {
    _sdaHi(); _sclHi(); delayMicroseconds(kHalfUs);
    _sdaLo();           delayMicroseconds(kHalfUs);
    _sclLo();           delayMicroseconds(kHalfUs);
  }
  void _stop() {
    _sdaLo();           delayMicroseconds(kHalfUs);
    _sclHi();           delayMicroseconds(kHalfUs);
    _sdaHi();           delayMicroseconds(kHalfUs);
  }

  bool _writeByte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
      if (b & 0x80) _sdaHi(); else _sdaLo();
      delayMicroseconds(kHalfUs);
      _sclHi(); delayMicroseconds(kHalfUs);
      _sclLo(); delayMicroseconds(kHalfUs);
      b <<= 1;
    }
    _sdaHi(); delayMicroseconds(kHalfUs);
    _sclHi(); delayMicroseconds(kHalfUs);
    bool ack = !_sdaRead();  // 0 = ACK
    _sclLo(); delayMicroseconds(kHalfUs);
    return ack;
  }

  uint8_t _readByte(bool ackAfter) {
    uint8_t v = 0;
    _sdaHi();
    for (uint8_t i = 0; i < 8; i++) {
      delayMicroseconds(kHalfUs);
      _sclHi(); delayMicroseconds(kHalfUs);
      v = (v << 1) | (_sdaRead() ? 1 : 0);
      _sclLo();
    }
    if (ackAfter) _sdaLo(); else _sdaHi();
    delayMicroseconds(kHalfUs);
    _sclHi(); delayMicroseconds(kHalfUs);
    _sclLo(); delayMicroseconds(kHalfUs);
    return v;
  }

  bool _readReg(uint8_t reg, uint8_t* buf, uint8_t len) {
    _start();
    if (!_writeByte(kAddr << 1)) { _stop(); return false; }
    if (!_writeByte(reg))        { _stop(); return false; }
    _start();  // repeated start
    if (!_writeByte((kAddr << 1) | 1)) { _stop(); return false; }
    for (uint8_t i = 0; i < len; i++) buf[i] = _readByte(i < (uint8_t)(len - 1));
    _stop();
    return true;
  }

  bool _writeReg(uint8_t reg, const uint8_t* buf, uint8_t len) {
    _start();
    if (!_writeByte(kAddr << 1)) { _stop(); return false; }
    if (!_writeByte(reg))        { _stop(); return false; }
    for (uint8_t i = 0; i < len; i++) {
      if (!_writeByte(buf[i])) { _stop(); return false; }
    }
    _stop();
    return true;
  }
};
