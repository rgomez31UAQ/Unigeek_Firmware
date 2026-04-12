#pragma once

#include "ui/templates/BaseScreen.h"
#include <NimBLEDevice.h>

class BLEAndroidSpamScreen : public BaseScreen {
public:
  const char* title()    override { return "Android Spam"; }
  bool inhibitPowerOff() override { return true; }

  ~BLEAndroidSpamScreen() override;
  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  static constexpr const char* _spinner = "-\\|/";
  uint8_t  _spinIdx     = 0;
  uint32_t _lastSpamMs  = 0;
  uint32_t _lastDrawMs  = 0;
  uint32_t _spamStartMs = 0;
  bool     _spam1minFired = false;

  NimBLEAdvertising* _pAdv = nullptr;

  void _spam();
  void _stop();
};