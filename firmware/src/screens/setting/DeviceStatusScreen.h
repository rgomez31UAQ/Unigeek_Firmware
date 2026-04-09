#pragma once

#include "ui/templates/ListViewScreen.h"

class DeviceStatusScreen : public ListViewScreen
{
public:
  const char* title() override { return "Device Status"; }

  void onInit() override;

protected:
  void onBack() override;

private:
  static constexpr uint8_t MAX_ROWS = 8;
  Row     _rows[MAX_ROWS] = {};
  uint8_t _rowCount = 0;

  // String storage for row values (must outlive render)
  String _cpuFreq;
  String _ramFree;
  String _psramFree;
  String _lfsFree;
  String _sdFree;
};
