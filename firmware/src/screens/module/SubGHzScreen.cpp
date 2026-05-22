#include "SubGHzScreen.h"
#include "core/AchievementManager.h"
#include "core/Device.h"
#include "core/PinConfigManager.h"
#include "core/ScreenManager.h"
#include "ui/actions/InputNumberAction.h"
#include "ui/actions/InputSelectAction.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/views/ProgressView.h"
#include "utils/rf/KeeloqKeystore.h"

void SubGHzScreen::onInit() {
  _csPin   = PinConfig.get(PIN_CONFIG_CC1101_CS,   PIN_CONFIG_CC1101_CS_DEFAULT).toInt();
  _gdo0Pin = PinConfig.get(PIN_CONFIG_CC1101_GDO0, PIN_CONFIG_CC1101_GDO0_DEFAULT).toInt();

  if (_csPin < 0 || _gdo0Pin < 0) {
    ShowStatusAction::show("Set CC1101 pins first");
    Screen.goBack();
    return;
  }

  ProgressView::init();
  ProgressView::progress("Detecting CC1101...", 30);
  if (!_rf.begin(Uni.Spi, _csPin, _gdo0Pin)) {
    ShowStatusAction::show("CC1101 not found!");
    Screen.goBack();
    return;
  }
  _rf.end();

  _showMenu();
}

// ── Radio adapter (chip lifecycle gated per operation) ──────────────────────

bool SubGHzScreen::_radioSendFromBrowse(const Signal& sig) {
  if (!_rf.begin(Uni.Spi, _csPin, _gdo0Pin)) return false;
  _rf.sendSignal(sig);
  _rf.end();
  return true;
}

void SubGHzScreen::_radioSendCaptured(const Signal& sig) {
  // Detach RX before TX — every pulse we drive on GDO0 would otherwise re-fire
  // the receive interrupt and corrupt the buffer. Caller re-arms RX via
  // _radioBeginReceive() in the base's onItemSelected().
  _rf.endReceive();
  _rf.sendSignal(sig);
}

bool SubGHzScreen::_radioStartJam() {
  if (!_rf.begin(Uni.Spi, _csPin, _gdo0Pin)) return false;
  _rf.startTx();
  return true;
}

void SubGHzScreen::_radioStopJam() {
  digitalWrite(_gdo0Pin, LOW);
  _rf.end();
}

void SubGHzScreen::_radioJamBurst() {
  for (int i = 0; i < 50; i++) {
    uint32_t pw  = 5 + (micros() % 46);
    uint32_t gap = 5 + (micros() % 96);
    digitalWrite(_gdo0Pin, HIGH); delayMicroseconds(pw);
    digitalWrite(_gdo0Pin, LOW);  delayMicroseconds(gap);
  }
}

// ── Menu ────────────────────────────────────────────────────────────────────

void SubGHzScreen::_showMenu() {
  _state = STATE_MENU;
  _chromeDrawn = false;
  strcpy(_titleBuf, "Sub-GHz");
  _updateSublabels();
  setItems(_menuItems, kMenuCount);
}

void SubGHzScreen::_updateSublabels() {
  char buf[12];
  snprintf(buf, sizeof(buf), "%.2f MHz", _rf.getFrequency());
  _freqSub = buf;
  _menuItems[0].sublabel = _freqSub.c_str();

  // Mfcodes status — lazy-loads /unigeek/mfcodes on first access.
  auto& store = KeeloqKeystore::instance();
  if (store.isLoaded()) {
    _mfcodesSub = String((unsigned)store.count()) + " keys";
  } else {
    _mfcodesSub = "not loaded";
  }
  _menuItems[5].sublabel = _mfcodesSub.c_str();
}

void SubGHzScreen::_reloadMfcodes() {
  auto& store = KeeloqKeystore::instance();
  store.reload();
  char msg[80];
  if (store.count() > 0) {
    snprintf(msg, sizeof(msg), "Loaded %u keys from %s",
             (unsigned)store.count(), KeeloqKeystore::PATH);
  } else {
    snprintf(msg, sizeof(msg), "No keys at %s", KeeloqKeystore::PATH);
  }
  ShowStatusAction::show(msg, 2500);
  _updateSublabels();
  render();
}

void SubGHzScreen::_onMenuSelected(uint8_t index) {
  switch (index) {
    case 0: { // Frequency
      _selectFrequency();
      return;
    }
    case 1: { // Detect Freq
      _startScan();
      return;
    }
    case 2: { // Receive
      if (_csPin < 0 || _gdo0Pin < 0) {
        ShowStatusAction::show("Set CS and GDO0 pins first");
        render();
        return;
      }
      if (!_rf.begin(Uni.Spi, _csPin, _gdo0Pin)) {
        ShowStatusAction::show("CC1101 not found");
        render();
        return;
      }
      _enterReceiveMode();
      return;
    }
    case 3: { // Send
      if (_csPin < 0) {
        ShowStatusAction::show("Set CS pin first");
        render();
        return;
      }
      _enterBrowseMode();
      return;
    }
    case 5: { // Mfcodes — reload + status popup
      _reloadMfcodes();
      return;
    }
    case 4: { // Jammer
      if (_csPin < 0 || _gdo0Pin < 0) {
        ShowStatusAction::show("Set CS and GDO0 pins first");
        render();
        return;
      }
      if (!_radioStartJam()) {
        ShowStatusAction::show("CC1101 not found");
        render();
        return;
      }
      _enterJammingMode();
      return;
    }
  }
}

void SubGHzScreen::_selectFrequency() {
  static constexpr InputSelectAction::Option freqOpts[] = {
    {"300 MHz",    "300"},
    {"315 MHz",    "315"},
    {"345 MHz",    "345"},
    {"390 MHz",    "390"},
    {"433.92 MHz", "433.92"},
    {"434 MHz",    "434"},
    {"868 MHz",    "868"},
    {"915 MHz",    "915"},
    {"Custom",     "custom"},
  };

  char curBuf[12];
  snprintf(curBuf, sizeof(curBuf), "%.2f", _rf.getFrequency());

  const char* choice = InputSelectAction::popup("Frequency", freqOpts, 9, curBuf);
  if (!choice) { render(); return; }

  float mhz;
  if (strcmp(choice, "custom") == 0) {
    int val = InputNumberAction::popup("MHz (280-928)", 280, 928, (int)_rf.getFrequency());
    if (InputNumberAction::wasCancelled()) { render(); return; }
    mhz = (float)val;
  } else {
    mhz = atof(choice);
  }

  if (!_rf.setFrequency(mhz)) {
    ShowStatusAction::show("Invalid frequency");
  }
  _updateSublabels();
  render();
}

// ── Frequency scan (STATE_SCANNING) ─────────────────────────────────────────

void SubGHzScreen::_startScan() {
  if (_csPin < 0 || _gdo0Pin < 0) {
    ShowStatusAction::show("Set CS and GDO0 pins first");
    render();
    return;
  }
  if (!_rf.begin(Uni.Spi, _csPin, _gdo0Pin)) {
    ShowStatusAction::show("CC1101 not found");
    render();
    return;
  }
  _rfDetectFired = false;
  _rf.beginScan();
  _state = STATE_SCANNING;
  _chromeDrawn = false;
  strcpy(_titleBuf, "Detect Freq");
  render();
}

bool SubGHzScreen::_onUpdateExtra() {
  if (_state != STATE_SCANNING) return false;
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _rf.endScan();
      _rf.end();
      _showMenu();
      return true;
    }
  }
  _rf.stepScan();
  if (!_rfDetectFired && _rf.getScanRssi() > CC1101Util::RSSI_THRESHOLD) {
    _rfDetectFired = true;
    int n = Achievement.inc("rf_detect_freq");
    if (n == 1) Achievement.unlock("rf_detect_freq");
  }
  render();
  return true;
}

bool SubGHzScreen::_onRenderExtra() {
  if (_state != STATE_SCANNING) return false;
  auto& lcd = Uni.Lcd;

  static constexpr int kRssiFloor   = -110;
  static constexpr int kRssiCeiling = -30;
  static constexpr int kRssiRange   = kRssiCeiling - kRssiFloor; // 80

  const int footerH = 16;
  const int infoH   = 26;
  const int contentH = bodyH() - footerH;
  const int chartY   = infoH;
  const int chartH   = contentH - infoH;

  if (!_chromeDrawn) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextDatum(MC_DATUM);
    lcd.fillRect(bodyX(), bodyY() + bodyH() - footerH, bodyW(), footerH, Config.getThemeColor());
    lcd.setTextColor(TFT_WHITE, Config.getThemeColor());
    #ifdef DEVICE_HAS_KEYBOARD
      lcd.drawString("BACK: Stop", bodyX() + bodyW() / 2, bodyY() + bodyH() - 8);
    #else
      lcd.drawString("< Stop", bodyX() + bodyW() / 2, bodyY() + bodyH() - 8);
    #endif
    _chromeDrawn = true;
  }

  uint8_t n = _rf.getScanCount();
  int barW  = bodyW() / (n ? n : 1);
  if (barW < 1) barW = 1;

  uint8_t bestIdx  = 0;
  int     bestRssi = -120;
  for (uint8_t i = 0; i < n; i++) {
    int r = _rf.getScanRssiAt(i);
    if (r > bestRssi) { bestRssi = r; bestIdx = i; }
  }

  Sprite sp(&lcd);
  sp.createSprite(bodyW(), contentH);
  sp.fillSprite(TFT_BLACK);
  sp.setTextSize(1);

  for (uint8_t i = 0; i < n; i++) {
    int rssi    = _rf.getScanRssiAt(i);
    int clamped = constrain(rssi, kRssiFloor, kRssiCeiling);
    int barH    = (clamped - kRssiFloor) * chartH / kRssiRange;
    int x       = i * barW;
    int y       = chartY + chartH - barH;

    uint16_t color;
    if (i == bestIdx && rssi > CC1101Util::RSSI_THRESHOLD)     color = TFT_YELLOW;
    else if (rssi > CC1101Util::RSSI_THRESHOLD)                color = TFT_GREEN;
    else if (rssi > kRssiFloor + 10)                           color = 0x2945;
    else                                                       color = TFT_DARKGREY;

    if (barH > 0) sp.fillRect(x, y, barW - 1, barH, color);
  }

  for (uint8_t i = 0; i < n; i++) {
    if (fabsf(_rf.getScanFreqAt(i) - _rf.getScanFreq()) < 0.01f) {
      sp.drawFastVLine(i * barW + barW / 2, chartY, chartH, TFT_WHITE);
      break;
    }
  }

  sp.setTextDatum(ML_DATUM);
  sp.setTextColor(TFT_WHITE, TFT_BLACK);
  char freqBuf[20];
  snprintf(freqBuf, sizeof(freqBuf), "%.3f MHz", _rf.getScanFreq());
  sp.drawString(freqBuf, 2, 7);

  sp.setTextDatum(MR_DATUM);
  char rssiBuf[16];
  snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", _rf.getScanRssi());
  uint16_t rssiColor = (_rf.getScanRssi() > CC1101Util::RSSI_THRESHOLD) ? TFT_GREEN : TFT_CYAN;
  sp.setTextColor(rssiColor, TFT_BLACK);
  sp.drawString(rssiBuf, bodyW() - 2, 7);

  sp.setTextDatum(ML_DATUM);
  if (bestRssi > CC1101Util::RSSI_THRESHOLD) {
    sp.setTextColor(TFT_YELLOW, TFT_BLACK);
    char bestBuf[28];
    snprintf(bestBuf, sizeof(bestBuf), "> %.3f MHz %ddBm", _rf.getScanFreqAt(bestIdx), bestRssi);
    sp.drawString(bestBuf, 2, 19);
  } else {
    sp.setTextColor(TFT_DARKGREY, TFT_BLACK);
    sp.drawString("No signal", 2, 19);
  }

  sp.pushSprite(bodyX(), bodyY());
  sp.deleteSprite();
  return true;
}

bool SubGHzScreen::_onBackExtra() {
  if (_state != STATE_SCANNING) return false;
  _rf.endScan();
  _rf.end();
  _showMenu();
  return true;
}
