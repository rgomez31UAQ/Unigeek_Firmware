#include "WifiAPScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/ConfigManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/ShowQRCodeAction.h"
#include "ui/actions/InputSelectOption.h"
#include <WiFi.h>

// Static callback bridge for visit logging
static WifiAPScreen* _activeInstance = nullptr;

static void _onVisit(const char* clientIP, const char* domain)
{
  if (_activeInstance) {
    char buf[60];
    snprintf(buf, sizeof(buf), "%s > %s", clientIP, domain);
    _activeInstance->logVisit(buf);
  }
}

static void _onPost(const char* clientIP, const char* domain, const char* data)
{
  (void)clientIP;
  (void)data;
  if (_activeInstance) {
    char buf[60];
    snprintf(buf, sizeof(buf), "[+] POST %s", domain);
    _activeInstance->logVisit(buf);
  }
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void WifiAPScreen::onInit()
{
  _activeInstance = this;
  _showMenu();
}

void WifiAPScreen::onUpdate()
{
  _rogueServer.update();

  if (_state == STATE_LOG && millis() - _lastDraw > 500) {
    _drawLog();
    _lastDraw = millis();
  }

  if (_state != STATE_LOG) {
    ListScreen::onUpdate();
  } else {
    // Handle back in log view
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
        _stopAP();
      }
    }
  }
}

void WifiAPScreen::onItemSelected(uint8_t index)
{
  if (_state != STATE_MENU) return;

  switch (index) {
    case 0: { // SSID
      String ssid = InputTextAction::popup("SSID", Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT));
      render();
      if (ssid.isEmpty()) {
        ShowStatusAction::show("SSID is required", 1500);
        render();
        return;
      }
      if (ssid.length() > 32) {
        ShowStatusAction::show("SSID too long (max 32)", 1500);
        render();
        return;
      }
      Config.set(APP_CONFIG_WIFI_AP_SSID, ssid);
      Config.save(Uni.Storage);
      _showMenu();
      break;
    }
    case 1: { // Password
      String pwd = InputTextAction::popup("Password", Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT));
      render();
      if (pwd.length() > 0 && pwd.length() < 8) {
        ShowStatusAction::show("Min 8 chars or empty for open", 1500);
        render();
        return;
      }
      if (pwd.length() > 63) {
        ShowStatusAction::show("Password too long (max 63)", 1500);
        render();
        return;
      }
      Config.set(APP_CONFIG_WIFI_AP_PASSWORD, pwd);
      Config.save(Uni.Storage);
      _showMenu();
      break;
    }
    case 2: { // Hidden toggle
      _hidden = !_hidden;
      _menuItems[2].sublabel = _hidden ? "Yes" : "No";
      render();
      break;
    }
    case 3: { // Rogue DNS toggle
      if (!_rogueEnabled) {
        if (!Uni.Storage || !Uni.Storage->exists(RogueDnsServer::CONFIG_PATH)) {
          ShowStatusAction::show("dns_config not found", 1500);
          render();
          break;
        }
      }
      _rogueEnabled = !_rogueEnabled;
      _menuItems[3].sublabel = _rogueEnabled ? "Yes" : "No";
      render();
      break;
    }
    case 4: { // Captive Portal toggle
      if (!_captiveEnabled) {
        // List portal folders
        static constexpr const char* PORTALS_DIR = "/unigeek/wifi/portals";
        if (!Uni.Storage || !Uni.Storage->exists(PORTALS_DIR)) {
          ShowStatusAction::show("No portals found", 1500);
          render();
          break;
        }
        IStorage::DirEntry entries[20];
        uint8_t count = Uni.Storage->listDir(PORTALS_DIR, entries, 20);
        // Filter to directories only
        InputSelectAction::Option opts[10];
        int optCount = 0;
        for (uint8_t i = 0; i < count && optCount < 10; i++) {
          if (!entries[i].isDir) continue;
          opts[optCount] = {entries[i].name.c_str(), entries[i].name.c_str()};
          optCount++;
        }
        if (optCount == 0) {
          ShowStatusAction::show("No portal folders found", 1500);
          render();
          break;
        }
        const char* selected = InputSelectAction::popup("Captive Portal", opts, optCount);
        render();
        if (!selected) break;
        _captivePath = String(PORTALS_DIR) + "/" + selected;
        _captiveEnabled = true;
        _captiveSub = selected;
      } else {
        _captiveEnabled = false;
        _captivePath = "";
        _captiveSub = "No";
      }
      _menuItems[4].sublabel = _captiveEnabled ? _captiveSub.c_str() : "No";
      render();
      break;
    }
    case 5: { // File Manager toggle
      if (!_fileManagerEnabled) {
        String indexPath = String(WebFileManager::WEB_PATH) + "/index.htm";
        if (!Uni.Storage || !Uni.Storage->exists(indexPath.c_str())) {
          ShowStatusAction::show("Web files not installed", 1500);
          render();
          break;
        }
      }
      _fileManagerEnabled = !_fileManagerEnabled;
      _menuItems[5].sublabel = _fileManagerEnabled ? "Yes" : "No";
      render();
      break;
    }
    case 6: { // Start
      _startAP();
      break;
    }
  }
}

void WifiAPScreen::onBack()
{
  if (_state == STATE_LOG) {
    _stopAP();
    return;
  }
  _activeInstance = nullptr;
  Screen.setScreen(new WifiMenuScreen());
}

// ── Private ────────────────────────────────────────────────────────────────

void WifiAPScreen::_showMenu()
{
  _state       = STATE_MENU;
  _ssidSub     = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
  String pwd   = Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT);
  _passwordSub = pwd.isEmpty() ? "None" : pwd;
  _captiveSub = _captiveEnabled ? _captivePath.substring(_captivePath.lastIndexOf('/') + 1) : "No";
  _menuItems[0] = {"SSID",            _ssidSub.c_str()};
  _menuItems[1] = {"Password",        _passwordSub.c_str()};
  _menuItems[2] = {"Hidden",          _hidden ? "Yes" : "No"};
  _menuItems[3] = {"Rogue DNS",       _rogueEnabled ? "Yes" : "No"};
  _menuItems[4] = {"Captive Portal",  _captiveSub.c_str()};
  _menuItems[5] = {"File Manager",    _fileManagerEnabled ? "Yes" : "No"};
  _menuItems[6] = {"Start"};
  setItems(_menuItems, 7);
}

void WifiAPScreen::_startAP()
{
  String ssid = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
  String pwd  = Config.get(APP_CONFIG_WIFI_AP_PASSWORD, APP_CONFIG_WIFI_AP_PASSWORD_DEFAULT);

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(
    IPAddress(10, 0, 0, 1),
    IPAddress(10, 0, 0, 1),
    IPAddress(255, 255, 255, 0)
  );
  WiFi.softAP(ssid.c_str(), pwd.c_str(), 1, _hidden);

  // Start Rogue DNS if enabled (also needed for captive portal)
  if (_rogueEnabled || _captiveEnabled) {
    _rogueServer.setVisitCallback(_onVisit);
    _rogueServer.setPostCallback(_onPost);
    if (_captiveEnabled) {
      _rogueServer.setCaptivePortalPath(_captivePath.c_str());
    }
    _rogueServer.setFileManagerEnabled(_fileManagerEnabled);
    if (!_rogueServer.begin(WiFi.softAPIP())) {
      _rogueEnabled = false;
      _captiveEnabled = false;
    }
  }

  // Start File Manager if enabled
  if (_fileManagerEnabled) {
    if (!_fileManager.begin()) {
      _fileManagerEnabled = false;
    }
  }

  _showLog();
}

void WifiAPScreen::_stopAP()
{
  if (_rogueEnabled || _captiveEnabled) {
    _rogueServer.end();
  }
  if (_fileManagerEnabled) {
    _fileManager.end();
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_STA);
  _rogueEnabled = false;
  _captiveEnabled = false;
  _fileManagerEnabled = false;
  ShowStatusAction::show("AP Stopped", 1500);
  _showMenu();
}

void WifiAPScreen::_showLog()
{
  _state = STATE_LOG;
  _logCount = 0;

  String ssid = Config.get(APP_CONFIG_WIFI_AP_SSID, APP_CONFIG_WIFI_AP_SSID_DEFAULT);
  char apLabel[60];
  snprintf(apLabel, sizeof(apLabel), "[*] AP: %s", ssid.c_str());
  _addLog(apLabel);

  if (_rogueEnabled) {
    _addLog("[*] Rogue DNS started");
    for (int i = 0; i < _rogueServer.recordCount(); i++) {
      char buf[60];
      const char* path = _rogueServer.records()[i].path;
      const char* lastSlash = strrchr(path, '/');
      const char* pathName = lastSlash ? lastSlash + 1 : path;
      snprintf(buf, sizeof(buf), "  %s > %s",
               _rogueServer.records()[i].domain, pathName);
      _addLog(buf);
    }
  }

  if (_captiveEnabled) {
    char capBuf[60];
    const char* lastSlash = strrchr(_captivePath.c_str(), '/');
    snprintf(capBuf, sizeof(capBuf), "[*] Captive: %s", lastSlash ? lastSlash + 1 : _captivePath.c_str());
    _addLog(capBuf);
  }

  if (_fileManagerEnabled) {
    _addLog("[*] File Manager: unigeek.local");
  }

  _addLog("");
  _addLog("Waiting for clients...");

  _drawLog();
}

// ── Log ────────────────────────────────────────────────────────────────────

void WifiAPScreen::logVisit(const char* msg)
{
  _addLog(msg);
}

void WifiAPScreen::_addLog(const char* msg)
{
  if (_logCount < MAX_LOG) {
    strncpy(_logLines[_logCount], msg, 59);
    _logLines[_logCount][59] = '\0';
    _logCount++;
  } else {
    for (int i = 0; i < MAX_LOG - 1; i++) {
      memcpy(_logLines[i], _logLines[i + 1], 60);
    }
    strncpy(_logLines[MAX_LOG - 1], msg, 59);
    _logLines[MAX_LOG - 1][59] = '\0';
  }
}

void WifiAPScreen::_drawLog()
{
  TFT_eSprite sp(&Uni.Lcd);
  sp.createSprite(bodyW(), bodyH());
  sp.fillSprite(TFT_BLACK);

  int lineH    = 10;
  int statusH  = 14;
  int logAreaH = bodyH() - statusH;
  int maxVisible = logAreaH / lineH;
  int startIdx   = _logCount > maxVisible ? _logCount - maxVisible : 0;

  sp.setTextDatum(TL_DATUM);
  sp.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = startIdx; i < _logCount; i++) {
    int y = (i - startIdx) * lineH;
    sp.drawString(_logLines[i], 2, y, 1);
  }

  // Status bar
  int sepY = bodyH() - statusH;
  sp.drawFastHLine(0, sepY, bodyW(), TFT_DARKGREY);

  int barY = sepY + 2;
  sp.setTextColor(TFT_GREEN, TFT_BLACK);
  sp.setTextDatum(TL_DATUM);

  char label[30];
  if (_rogueEnabled) {
    snprintf(label, sizeof(label), "DNS: %d", _rogueServer.recordCount());
  } else {
    snprintf(label, sizeof(label), "AP");
  }
  sp.drawString(label, 2, barY, 1);

  sp.setTextDatum(TR_DATUM);
  sp.drawString(WiFi.softAPIP().toString(), bodyW() - 2, barY, 1);

  sp.pushSprite(bodyX(), bodyY());
  sp.deleteSprite();
}
