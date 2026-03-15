#pragma once
#include "ui/templates/ListScreen.h"

class DownloadScreen : public ListScreen {
public:
  const char* title() override { return "Download"; }

  void onInit() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  ListItem _menuItems[2];
  String   _wfmVersionSub;

  void _showMenu();
  void _downloadWebPage();
  void _downloadSampleData();
  bool _downloadFile(const char* url, const char* path);

  static constexpr const char* REPO_BASE =
    "https://raw.githubusercontent.com/lshaf/unigeek/main/sdcard";
};
