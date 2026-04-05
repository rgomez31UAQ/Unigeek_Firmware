//
// ExtSpiClass — SPIClass wrapper that remembers its configured pins.
// ESP32 Arduino 2.x SPIClass does not expose pinSCK/MISO/MOSI accessors,
// so we store them ourselves on begin() and expose them for peripheral drivers.
//

#pragma once
#include <SPI.h>

class ExtSpiClass : public SPIClass {
public:
  // Inherit all SPIClass constructors (bus number)
  using SPIClass::SPIClass;

  // Override begin to capture pin numbers before forwarding to base class
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) {
    _sck  = sck;
    _miso = miso;
    _mosi = mosi;
    _ss   = ss;
    SPIClass::begin(sck, miso, mosi, ss);
  }

  int8_t pinSCK()  const { return _sck;  }
  int8_t pinMISO() const { return _miso; }
  int8_t pinMOSI() const { return _mosi; }
  int8_t pinSS()   const { return _ss;   }

private:
  int8_t _sck  = -1;
  int8_t _miso = -1;
  int8_t _mosi = -1;
  int8_t _ss   = -1;
};
