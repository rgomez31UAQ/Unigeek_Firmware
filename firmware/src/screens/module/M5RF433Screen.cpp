#include "M5RF433Screen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "ui/actions/ShowStatusAction.h"
#include "utils/rf/KeeloqKeystore.h"

// Pinout is fixed per board at build time. M5 RF433T/R Units plug into the
// Grove port — default to GROVE_SDA (TX) / GROVE_SCL (RX). A board can override
// either with -DM5RF433_TX_PIN / -DM5RF433_RX_PIN in pins_arduino.h.
#if defined(M5RF433_TX_PIN)
  static constexpr int8_t kDefaultTxPin = (int8_t)M5RF433_TX_PIN;
#elif defined(GROVE_SDA)
  static constexpr int8_t kDefaultTxPin = (int8_t)GROVE_SDA;
#else
  static constexpr int8_t kDefaultTxPin = -1;
#endif

#if defined(M5RF433_RX_PIN)
  static constexpr int8_t kDefaultRxPin = (int8_t)M5RF433_RX_PIN;
#elif defined(GROVE_SCL)
  static constexpr int8_t kDefaultRxPin = (int8_t)GROVE_SCL;
#else
  static constexpr int8_t kDefaultRxPin = -1;
#endif

void M5RF433Screen::onInit() {
  _txPin = kDefaultTxPin;
  _rxPin = kDefaultRxPin;

  if (_txPin < 0 && _rxPin < 0) {
    ShowStatusAction::show("M5 RF433 not supported");
    Screen.goBack();
    return;
  }

  _rf.begin(_txPin, _rxPin);
  _showMenu();
}

bool M5RF433Screen::_radioBeginReceive() {
  if (_rxPin < 0) return false;
  return _rf.beginReceive();
}

bool M5RF433Screen::_radioSendFromBrowse(const Signal& sig) {
  if (_txPin < 0) return false;
  _rf.sendSignal(sig);
  return true;
}

void M5RF433Screen::_showMenu() {
  _state = STATE_MENU;
  _chromeDrawn = false;
  strcpy(_titleBuf, "M5 RF433");
  _updateMfcodesSub();
  setItems(_menuItems, kMenuCount);
}

void M5RF433Screen::_updateMfcodesSub() {
  auto& store = KeeloqKeystore::instance();
  if (store.isLoaded()) {
    _mfcodesSub = String((unsigned)store.count()) + " keys";
  } else {
    _mfcodesSub = "not loaded";
  }
  _menuItems[3].sublabel = _mfcodesSub.c_str();
}

void M5RF433Screen::_reloadMfcodes() {
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
  _updateMfcodesSub();
  render();
}

void M5RF433Screen::_onMenuSelected(uint8_t index) {
  switch (index) {
    case 0: { // Receive
      if (_rxPin < 0) {
        ShowStatusAction::show("RX pin not available");
        render();
        return;
      }
      _enterReceiveMode();
      break;
    }
    case 1: { // Send
      if (_txPin < 0) {
        ShowStatusAction::show("TX pin not available");
        render();
        return;
      }
      _enterBrowseMode();
      break;
    }
    case 2: { // Jammer
      if (_txPin < 0) {
        ShowStatusAction::show("TX pin not available");
        render();
        return;
      }
      _radioStartJam();
      _enterJammingMode();
      break;
    }
    case 3: { // Mfcodes — reload + status popup
      _reloadMfcodes();
      return;
    }
  }
}
