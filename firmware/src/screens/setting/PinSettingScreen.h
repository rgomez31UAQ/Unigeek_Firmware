#pragma once

#include "ui/templates/ListScreen.h"

class PinSettingScreen : public ListScreen
{
public:
  const char* title() override { return "Pin Setting"; }

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onBack() override;

private:
  void _refresh();

  String _sdaSub;
  String _sclSub;

  ListItem _items[2] = {
    {"External SDA", ""},
    {"External SCL", ""},
  };
};