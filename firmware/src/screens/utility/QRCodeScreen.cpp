#include "QRCodeScreen.h"
#include "core/ScreenManager.h"
#include "screens/utility/UtilityMenuScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/ShowQRCodeAction.h"
#include <SD.h>
#include <LittleFS.h>

void QRCodeScreen::onInit() {
  _state = STATE_MENU;
  _refreshMenu();
  setItems(_menuItems);
}

void QRCodeScreen::onBack() {
  if (_state == STATE_SELECT_FILE) {
    if (_currentPath == _qrPath) {
      _state = STATE_MENU;
      _refreshMenu();
      setItems(_menuItems);
    } else {
      int last = _currentPath.lastIndexOf('/');
      _currentPath = (last > 0) ? _currentPath.substring(0, last) : String(_qrPath);
      _scanFiles(_currentPath);
    }
    return;
  }
  Screen.setScreen(new UtilityMenuScreen());
}

void QRCodeScreen::onItemSelected(uint8_t index) {
  if (_state == STATE_MENU) {
    switch (index) {
      case 0:
        _mode = (_mode == MODE_WRITE) ? MODE_FILE : MODE_WRITE;
        _refreshMenu();
        render();
        break;
      case 1:
        _inverted = !_inverted;
        _refreshMenu();
        render();
        break;
      case 2:
        if (_mode == MODE_WRITE) {
          _generate();
        } else {
          if (!Uni.Storage || !Uni.Storage->isAvailable()) {
            ShowStatusAction::show("No storage available", 1500);
            render();
            return;
          }
          Uni.Storage->makeDir(_qrPath);
          _currentPath = _qrPath;
          _scanFiles(_currentPath);
        }
        break;
    }
  } else if (_state == STATE_SELECT_FILE) {
    if (index >= _fileCount) return;
    String path = _currentPath + "/" + _fileEntries[index].name;
    if (_fileEntries[index].isDir) {
      _currentPath = path;
      _scanFiles(_currentPath);
    } else {
      _generateFromFile(path);
    }
  }
}

void QRCodeScreen::_refreshMenu() {
  _menuItems[0].sublabel = (_mode == MODE_WRITE) ? "Write" : "File";
  _menuItems[1].sublabel = _inverted ? "Yes" : "No";
}

void QRCodeScreen::_generate() {
  String text = InputTextAction::popup("QR Content");
  if (text.length() == 0) { render(); return; }
  ShowQRCodeAction::show(text.c_str(), text.c_str(), _inverted);
  render();
}

void QRCodeScreen::_scanFiles(const String& path) {
  _state = STATE_SELECT_FILE;
  _fileCount = 0;

  bool useSD = Uni.StorageSD && Uni.StorageSD->isAvailable();
  File dir = useSD ? SD.open(path.c_str()) : LittleFS.open(path.c_str());
  if (!dir) {
    ShowStatusAction::show("Cannot open directory", 1500);
    _state = STATE_MENU;
    render();
    return;
  }

  File f;
  while ((f = dir.openNextFile()) && _fileCount < MAX_FILES) {
    bool isDir = f.isDirectory();
    strncpy(_fileEntries[_fileCount].name, f.name(), sizeof(_fileEntries[0].name) - 1);
    _fileEntries[_fileCount].name[sizeof(_fileEntries[0].name) - 1] = '\0';
    _fileEntries[_fileCount].isDir = isDir;
    _fileItems[_fileCount] = { _fileEntries[_fileCount].name, isDir ? "DIR" : "FILE" };
    _fileCount++;
    f.close();
  }
  dir.close();

  setItems(_fileItems, _fileCount);
}

void QRCodeScreen::_generateFromFile(const String& path) {
  bool useSD = Uni.StorageSD && Uni.StorageSD->isAvailable();
  File f = useSD ? SD.open(path.c_str()) : LittleFS.open(path.c_str());
  if (!f) {
    ShowStatusAction::show("Cannot open file", 1500);
    render();
    return;
  }
  if (f.size() > 1800) {
    f.close();
    ShowStatusAction::show("File too large", 1500);
    render();
    return;
  }

  String data = f.readString();
  f.close();

  ShowQRCodeAction::show(path.c_str(), data.c_str(), _inverted);
  render();
}
