#pragma once

#include "ui/templates/BaseScreen.h"

class BleFileManagerScreen : public BaseScreen {
public:
  ~BleFileManagerScreen();
  const char* title() override { return "BLE File Manager"; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

  bool inhibitPowerSave() override { return true; }

private:
  bool _lastConnected = false;
  bool _firstRender   = true;
};
