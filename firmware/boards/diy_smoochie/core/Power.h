//
// DIY Smoochie power — BQ25896 charger IC on Grove I2C (SDA=47, SCL=48).
// Battery voltage read from BATV register, wake from deep sleep on BTN_SEL.
//

#pragma once

#include "core/IPower.h"
#include "pins_arduino.h"
#include <Wire.h>

// ─── BQ25896 Charger / Power Path ─────────────────────────
#define BQ25896_ADDR        0x6B
#define BQ25896_REG_BATV    0x0E   // bits[6:0]: BATV; Vbat = BATV*20 + 2304 (mV)
#define BQ25896_REG_STATUS  0x0B   // bits[4:3]: CHRG_STAT
#define BQ25896_REG_ADC     0x02   // CONV_RATE / ADC enable

class PowerImpl : public IPower
{
public:
  void begin() override {
    Wire.begin(GROVE_SDA, GROVE_SCL);
    Wire.setClock(400000);

    // reset to defaults
    _writeReg(BQ25896_ADDR, 0x14, 0x80);
    delay(10);

    // enable continuous ADC measurement (CONV_RATE=1), ICO_EN=1, FORCE_VINDPM=1
    _writeReg(BQ25896_ADDR, BQ25896_REG_ADC, 0x3D);

    // charge target voltage 4208mV: VREG = (4208-3840)/16 = 23 → REG06[7:2] = 23 << 2 = 0x5C
    _writeReg(BQ25896_ADDR, 0x06, 0x5C);

    // fast charge current 832mA: ICHG = 832/64 = 13 = 0x0D
    _writeReg(BQ25896_ADDR, 0x04, 0x0D);
  }

  uint8_t getBatteryPercentage() override {
    uint8_t raw = _readReg8(BQ25896_ADDR, BQ25896_REG_BATV) & 0x7F;
    int mv  = (int)raw * 20 + 2304;
    int pct = (mv - 3300) * 100 / (4150 - 3350);
    if (pct < 0)   pct = 1;
    if (pct > 100) pct = 100;

    uint8_t chrg = (_readReg8(BQ25896_ADDR, BQ25896_REG_STATUS) >> 3) & 0x03;
    if ((chrg == 0x01 || chrg == 0x02) && pct >= 97) pct = 95;
    if (chrg == 0x03) pct = 100;  // charge termination done

    return (uint8_t)pct;
  }

  bool isCharging() override {
    uint8_t chrg = (_readReg8(BQ25896_ADDR, BQ25896_REG_STATUS) >> 3) & 0x03;
    return (chrg == 0x01 || chrg == 0x02);
  }

  void powerOff() override {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_SEL, LOW);
    esp_deep_sleep_start();
  }

private:
  uint8_t _readReg8(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }

  void _writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
};