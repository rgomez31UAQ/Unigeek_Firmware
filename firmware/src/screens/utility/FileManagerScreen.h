#pragma once

#include "ui/templates/ListScreen.h"

class FileManagerScreen : public ListScreen
{
public:
  const char* title() override { return _titleBuf; }

  void onInit() override;
  void onUpdate() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  enum State { STATE_FILE, STATE_MENU } _state = STATE_FILE;

  enum MenuAction {
    ACT_NEW_FOLDER, ACT_RENAME, ACT_DELETE,
    ACT_COPY, ACT_CUT, ACT_PASTE, ACT_CANCEL_CLIP, ACT_CLOSE_MENU, ACT_EXIT,
  };

  static constexpr uint8_t kMaxFiles = 40;
  static constexpr uint8_t kMaxMenu  = 9;

  // File browser
  String   _curPath = "/";
  String   _fileName[kMaxFiles];
  String   _filePath[kMaxFiles];
  bool     _fileIsDir[kMaxFiles];
  ListItem _fileItems[kMaxFiles];
  uint8_t  _fileCount  = 0;
  uint8_t  _menuSelIdx = 0;
  bool     _holdFired  = false;

  // Clipboard
  String _clipPath;
  String _clipOp;

  // Context menu
  MenuAction _menuActions[kMaxMenu];
  ListItem   _menuItems[kMaxMenu];
  uint8_t    _menuCount = 0;

  // Dynamic title (folder name)
  char _titleBuf[64] = "File Manager";

  void _loadDir(const String& path);
  void _openMenu(uint8_t fileIdx);
  void _handleMenuAction(uint8_t index);
  bool _removeDir(const String& path);
  void _updateTitle();
};