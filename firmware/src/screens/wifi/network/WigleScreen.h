#pragma once

#include "ui/templates/ListScreen.h"
#include "ui/components/ScrollListView.h"
#include "utils/gps/WigleUtil.h"

class WigleScreen : public ListScreen
{
public:
  const char* title() override { return "Wigle"; }

  void onInit() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_STATS,
    STATE_UPLOAD,
  } _state = STATE_MENU;

  String _tokenSub;

  ListItem _menuItems[3] = {
    {"Wigle Token"},
    {"Wardrive Stat"},
    {"Upload Wardrive"},
  };

  // Stats
  ScrollListView _statsView;
  ScrollListView::Row _statsRows[WigleUtil::MAX_STAT_ROWS];

  // Upload
  ListItem _uploadItems[WigleUtil::MAX_FILES];
  String _fileNames[WigleUtil::MAX_FILES];
  String _fileLabels[WigleUtil::MAX_FILES];
  bool _fileUploaded[WigleUtil::MAX_FILES];
  uint8_t _fileCount = 0;

  void _showMenu();
  void _editToken();
  void _showStats();
  void _showUploadMenu();
  void _uploadFile(uint8_t index);
};

