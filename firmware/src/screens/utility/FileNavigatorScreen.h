#pragma once

#include "ui/templates/ListScreen.h"

class FileNavigatorScreen : public ListScreen
{
public:
  const char* title() override { return _titleBuf; }

  void onInit() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  enum State { STATE_FILE, STATE_MENU } _state = STATE_FILE;

  enum MenuAction {
    ACT_NEW_FOLDER, ACT_RENAME, ACT_DELETE,
    ACT_COPY, ACT_CUT, ACT_PASTE, ACT_CANCEL, ACT_EXIT,
  };

  static constexpr uint8_t kMaxFiles = 40;
  static constexpr uint8_t kMaxMenu  = 8;

  // File browser
  String   _curPath = "/";
  String   _fileName[kMaxFiles];
  String   _filePath[kMaxFiles];
  bool     _fileIsDir[kMaxFiles];
  ListItem _fileItems[kMaxFiles];
  uint8_t  _fileCount  = 0;
  uint8_t  _menuSelIdx = 0;

  // Clipboard
  String _clipPath;
  String _clipOp;

  // Context menu
  MenuAction _menuActions[kMaxMenu];
  ListItem   _menuItems[kMaxMenu];
  uint8_t    _menuCount = 0;

  // Dynamic title (folder name)
  char _titleBuf[64] = "Files";

  void _loadDir(const String& path);
  void _openMenu(uint8_t fileIdx);
  void _handleMenuAction(uint8_t index);
  bool _removeDir(const String& path);
  void _updateTitle();
};