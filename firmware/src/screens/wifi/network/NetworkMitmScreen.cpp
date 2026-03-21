#include "NetworkMitmScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "screens/wifi/network/NetworkMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"
#include "utils/StorageUtil.h"
#include <WiFi.h>

NetworkMitmScreen* NetworkMitmScreen::_instance = nullptr;

// ── Lifecycle ───────────────────────────────────────────────────────────────

void NetworkMitmScreen::onInit()
{
  _showMenu();
}

void NetworkMitmScreen::onBack()
{
  if (_state == STATE_RUNNING) {
    _stop();
    _showMenu();
  } else {
    Screen.setScreen(new NetworkMenuScreen());
  }
}

void NetworkMitmScreen::onItemSelected(uint8_t index)
{
  if (_state != STATE_MENU) return;

  switch (index) {
    case 0: // Rogue DHCP
      _rogueEnabled = !_rogueEnabled;
      _rogueSub = _rogueEnabled ? "On" : "Off";
      _menuItems[0].sublabel = _rogueSub.c_str();
      render();
      break;

    case 1: { // DNS Spoof
      if (!_dnsEnabled) {
        if (!Uni.Storage || !Uni.Storage->exists(DnsSpoofServer::CONFIG_PATH)) {
          ShowStatusAction::show("dns_config not found", 1500);
          render();
          break;
        }
      }
      _dnsEnabled = !_dnsEnabled;
      _dnsSub = _dnsEnabled ? "On" : "Off";
      _menuItems[1].sublabel = _dnsSub.c_str();
      render();
      break;
    }

    case 2: { // File Manager
      if (!_fmEnabled) {
        if (!Uni.Storage || !Uni.Storage->exists(WebFileManager::WEB_PATH)) {
          ShowStatusAction::show("File manager not\ndownloaded", 1500);
          render();
          break;
        }
      }
      _fmEnabled = !_fmEnabled;
      _fmSub = _fmEnabled ? "On" : "Off";
      _menuItems[2].sublabel = _fmSub.c_str();
      render();
      break;
    }

    case 3: // DHCP Starvation
      _starvEnabled = !_starvEnabled;
      _starvSub = _starvEnabled ? "On" : "Off";
      _menuItems[3].sublabel = _starvSub.c_str();
      render();
      break;

    case 4: // Start
      _start();
      break;
  }
}

void NetworkMitmScreen::onUpdate()
{
  if (_state != STATE_RUNNING) {
    ListScreen::onUpdate();
    return;
  }

  // Exit
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _stop();
      _showMenu();
      return;
    }
  }

  // DNS Spoof
  if (_dnsEnabled) {
    _dnsSpoof.update();
  }

  // Rogue DHCP (only runs after starvation is done)
  if (_rogueEnabled && !_starvRunning) {
    _rogueDhcp.update();
  }

  // DHCP Starvation (non-blocking: one step per frame)
  if (_starvRunning) {
    _starv.step();

    if (_starv.isExhausted()) {
      _starvRunning = false;
      _starv.stop();

      const auto& s = _starv.stats();
      char buf[60];
      if (Uni.Speaker) Uni.Speaker->playWin();
      snprintf(buf, sizeof(buf), "[+] Pool exhausted! ACK:%lu NAK:%lu",
               (unsigned long)s.ack, (unsigned long)s.nak);
      _addLog(buf);

      // Chain: start Rogue DHCP now
      _startRogueDhcp();
    } else if (_starv.isStuck()) {
      _starvRunning = false;
      _starv.stop();

      if (Uni.Speaker) Uni.Speaker->playLose();
      _addLog("[!] Starvation failed (stuck)");

      // Still start Rogue DHCP if enabled
      _startRogueDhcp();
    }
  }

  // Redraw
  if (millis() - _lastDraw > 800) {
    // Update starvation progress in status bar
    _drawLog();
    _lastDraw = millis();
  }
}

// ── Menu ────────────────────────────────────────────────────────────────────

void NetworkMitmScreen::_showMenu()
{
  _state = STATE_MENU;
  _rogueSub  = _rogueEnabled ? "On" : "Off";
  _dnsSub    = _dnsEnabled ? "On" : "Off";
  _fmSub     = _fmEnabled ? "On" : "Off";
  _starvSub  = _starvEnabled ? "On" : "Off";

  _menuItems[0] = {"Rogue DHCP",       _rogueSub.c_str()};
  _menuItems[1] = {"DNS Spoof",        _dnsSub.c_str()};
  _menuItems[2] = {"File Manager",     _fmSub.c_str()};
  _menuItems[3] = {"DHCP Starvation",  _starvSub.c_str()};
  _menuItems[4] = {"Start"};
  setItems(_menuItems, 5);
}

// ── Start ───────────────────────────────────────────────────────────────────

void NetworkMitmScreen::_start()
{
  if (!_rogueEnabled && !_dnsEnabled && !_fmEnabled && !_starvEnabled) {
    ShowStatusAction::show("Enable at least\none option!");
    return;
  }
  if (_dnsEnabled && !StorageUtil::hasSpace()) {
    ShowStatusAction::show("Storage full! (<20KB free)");
    return;
  }

  _state        = STATE_RUNNING;
  _logCount     = 0;
  _lastDraw     = 0;
  _starvRunning = false;
  _instance     = this;

  IPAddress localIP = WiFi.localIP();
  char ipBuf[40];
  snprintf(ipBuf, sizeof(ipBuf), "[*] IP: %s", localIP.toString().c_str());
  _addLog(ipBuf);

  // 1. Start Rogue DHCP (deferred if starvation is also enabled)
  if (_rogueEnabled && !_starvEnabled) {
    _startRogueDhcp();
  }

  // 2. Start DNS Spoof
  if (_dnsEnabled) {
    _dnsSpoof.setUpstreamDns(WiFi.dnsIP());
    _dnsSpoof.setCaptiveIntercept(false);
    _dnsSpoof.setVisitCallback(_onDnsVisit);
    _dnsSpoof.setPostCallback(_onDnsPost);
    _dnsSpoof.setFileManagerEnabled(_fmEnabled);
    if (_dnsSpoof.begin(localIP)) {
      _addLog("[+] DNS Spoof active");
      for (int i = 0; i < _dnsSpoof.recordCount(); i++) {
        char buf[60];
        const char* path = _dnsSpoof.records()[i].path;
        const char* lastSlash = strrchr(path, '/');
        const char* name = lastSlash ? lastSlash + 1 : path;
        snprintf(buf, sizeof(buf), "    %s > %s",
                 _dnsSpoof.records()[i].domain, name);
        _addLog(buf);
      }
    } else {
      _addLog("[!] DNS Spoof failed");
      _dnsEnabled = false;
    }
  }

  // 3. Start File Manager
  if (_fmEnabled) {
    if (_fileManager.begin()) {
      if (_dnsEnabled) {
        _addLog("[+] File Manager: unigeek.local");
      } else {
        _addLog("[+] File Manager on :8080");
      }
    } else {
      _addLog("[!] File Manager failed");
      _fmEnabled = false;
    }
  }

  // 4. Start DHCP Starvation (last, interruptible)
  if (_starvEnabled) {
    _addLog("[*] DHCP Starvation...");
    _drawLog();
    if (_starv.begin()) {
      _starvRunning = true;
      char ipBuf2[40];
      snprintf(ipBuf2, sizeof(ipBuf2), "[+] Reconnected: %s",
               WiFi.localIP().toString().c_str());
      _addLog(ipBuf2);
    } else {
      _addLog("[!] Starvation failed");
      _starvEnabled = false;
    }
  }

  _addLog("BACK/Press to stop");
  _drawLog();
}

// ── Start Rogue DHCP (called after starvation or directly) ──────────────────

void NetworkMitmScreen::_startRogueDhcp()
{
  if (!_rogueEnabled) return;

  _rogueDhcp.setClientCallback(_onDhcpClient);
  if (_rogueDhcp.begin()) {
    _addLog("[+] Rogue DHCP active");
    _addLog("    Gateway + DNS = us");
  } else {
    _addLog("[!] Rogue DHCP failed");
    _rogueEnabled = false;
  }
}

// ── Stop ────────────────────────────────────────────────────────────────────

void NetworkMitmScreen::_stop()
{
  if (_starvRunning) {
    _starv.stop();
    _starvRunning = false;
  }
  if (_rogueEnabled) {
    _rogueDhcp.stop();
  }
  if (_dnsEnabled) {
    _dnsSpoof.end();
  }
  if (_fmEnabled) {
    _fileManager.end();
  }

  _instance = nullptr;
  _rogueEnabled = false;
  _dnsEnabled = false;
  _fmEnabled = false;
  _starvEnabled = false;
  _logCount = 0;
  _lastDraw = 0;

  ShowStatusAction::show("Stopped", 1000);
}

// ── Callbacks ───────────────────────────────────────────────────────────────

void NetworkMitmScreen::_onDnsVisit(const char* clientIP, const char* domain)
{
  if (!_instance) return;
  char buf[60];
  snprintf(buf, sizeof(buf), "[>] %s %s", clientIP, domain);
  _instance->_addLog(buf);
}

void NetworkMitmScreen::_onDnsPost(const char* clientIP, const char* domain, const char* data)
{
  if (!_instance) return;
  char buf[60];
  snprintf(buf, sizeof(buf), "[+] POST %s", domain);
  _instance->_addLog(buf);
  if (Uni.Speaker) Uni.Speaker->playNotification();
}

void NetworkMitmScreen::_onDhcpClient(const char* mac, const char* ip)
{
  if (!_instance) return;
  char buf[60];
  snprintf(buf, sizeof(buf), "[+] DHCP %s", ip);
  _instance->_addLog(buf);
  if (Uni.Speaker) Uni.Speaker->playNotification();
}

// ── Log ─────────────────────────────────────────────────────────────────────

void NetworkMitmScreen::_addLog(const char* msg)
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

void NetworkMitmScreen::_drawLog()
{
  TFT_eSprite sp(&Uni.Lcd);
  sp.createSprite(bodyW(), bodyH());
  sp.fillSprite(TFT_BLACK);

  int lineH = 10;
  int statusH = 14;
  int logAreaH = bodyH() - statusH;
  int maxVisible = logAreaH / lineH;
  int startIdx = _logCount > maxVisible ? _logCount - maxVisible : 0;

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

  char label[40];
  if (_starvRunning) {
    const auto& s = _starv.stats();
    snprintf(label, sizeof(label), "A:%lu N:%lu T:%lu CT:%d/20",
             (unsigned long)s.ack, (unsigned long)s.nak,
             (unsigned long)s.timeout, _starv.consecutiveTimeouts());
  } else if (_rogueEnabled) {
    snprintf(label, sizeof(label), "DHCP: %d clients", _rogueDhcp.clientCount());
  } else {
    snprintf(label, sizeof(label), "Running");
  }
  sp.drawString(label, 2, barY, 1);

  sp.pushSprite(bodyX(), bodyY());
  sp.deleteSprite();
}
