#pragma once

#include "ui/templates/BaseScreen.h"
#include "ui/views/ScrollListView.h"

class AboutScreen : public BaseScreen
{
public:
  const char* title() override { return "About"; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  static constexpr uint8_t MAX_ROWS = 14;
  ScrollListView       _view;
  ScrollListView::Row  _rows[MAX_ROWS];
  uint8_t              _rowCount = 0;
};