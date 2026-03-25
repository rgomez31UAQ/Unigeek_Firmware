#pragma once

#include "ui/templates/BaseScreen.h"
#include "ui/components/ScrollListView.h"

class DeviceStatusScreen : public BaseScreen
{
public:
  const char* title() override { return "Device Status"; }

  void onInit() override;
  void onUpdate() override;

private:
  static constexpr uint8_t MAX_ROWS = 8;
  ScrollListView _view;
  ScrollListView::Row _rows[MAX_ROWS];
  uint8_t _rowCount = 0;

  // String storage for row values (must outlive render)
  String _cpuFreq;
  String _ramFree;
  String _psramFree;
  String _lfsFree;
  String _sdFree;
};
