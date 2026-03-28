//
// Created by L Shaf on 2026-03-26.
//

#include "GPSScreen.h"

#include <WiFi.h>
#include <Wire.h>

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/PinConfigManager.h"
#include "screens/module/ModuleMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/ShowProgressAction.h"
#include <sys/time.h>
#ifdef DEVICE_HAS_RTC
#include "core/RtcManager.h"
#endif

void GPSScreen::onInit() {
  _initTime = millis();

  // load saved or default GPS config
  _txPin = (int8_t)PinConfig.getInt(PIN_CONFIG_GPS_TX, PIN_CONFIG_GPS_TX_DEFAULT);
  _rxPin = (int8_t)PinConfig.getInt(PIN_CONFIG_GPS_RX, PIN_CONFIG_GPS_RX_DEFAULT);
  _baudRate = (uint32_t)PinConfig.getInt(PIN_CONFIG_GPS_BAUD, PIN_CONFIG_GPS_BAUD_DEFAULT);
  if (_baudRate == 0) _baudRate = 9600;

  _enableGnssPower();
  _gps.begin(2, _baudRate, _rxPin, _txPin);
  _state = STATE_LOADING;
}

void GPSScreen::onUpdate() {
  _gps.update();

  if (_state == STATE_LOADING) {
    // Re-render to animate spinner
    if (millis() - _lastRender > 250) {
      _lastRender = millis();
      render();
    }
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
        ShowStatusAction::show("Stopping GPS...", 0);
        _gps.end();
        _disableGnssPower();
        Screen.setScreen(new ModuleMenuScreen());
      }
    }
    if (_gps.gps.location.isValid()) {
      // Sync device time from GPS (UTC)
      auto& d = _gps.gps.date;
      auto& t = _gps.gps.time;
      if (d.isValid() && t.isValid() && d.year() >= 2020) {
        struct tm tm = {};
        tm.tm_year = d.year() - 1900;
        tm.tm_mon  = d.month() - 1;
        tm.tm_mday = d.day();
        tm.tm_hour = t.hour();
        tm.tm_min  = t.minute();
        tm.tm_sec  = t.second();
        time_t epoch = mktime(&tm);
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
#ifdef DEVICE_HAS_RTC
        RtcManager::syncRtcFromSystem();
#endif
      }
      _showMenu();
      return;
    }
    if (millis() - _initTime > 5000 && _gps.gps.charsProcessed() < 10) {
      ShowStatusAction::show("GPS not detected!\nCheck connection");
      _gps.end();
      _disableGnssPower();
      Screen.setScreen(new ModuleMenuScreen());
      return;
    }
    return;
  }

  if (_state == STATE_INFO) {
    if (millis() - _lastRender > 1000) _renderInfo();
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
        _showMenu();
      } else {
        _infoView.onNav(dir);
      }
    }
    return;
  }

  if (_state == STATE_WARDRIVING) {
    _gps.doWardrive(Uni.Storage);
    if (millis() - _lastRender > 1000) _renderWardriver();
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
        ShowStatusAction::show("Stopping wardrive...", 0);
        _gps.endWardrive();
        _showMenu();
      }
    }
    return;
  }

  // STATE_MENU — default list behavior
  ListScreen::onUpdate();
}

void GPSScreen::onRender() {
  if (_state == STATE_LOADING) {
    TFT_eSprite sp(&Uni.Lcd);
    sp.createSprite(bodyW(), bodyH());
    sp.fillSprite(TFT_BLACK);
    sp.setTextDatum(MC_DATUM);
    sp.setTextSize(1);
    bool gpsFound = _gps.gps.charsProcessed() >= 10;
    static const char spinner[] = {'/', '-', '\\', '|'};
    uint8_t frame = (millis() / 250) % 4;
    char anim[4] = {'[', spinner[frame], ']', '\0'};

    sp.setTextColor(TFT_WHITE);
    if (gpsFound) {
      sp.drawString("GPS module found!", bodyW() / 2, bodyH() / 2 - 20);
      sp.setTextColor(TFT_DARKGREY);
      sp.drawString("Acquiring satellite fix...", bodyW() / 2, bodyH() / 2 - 4);
      sp.drawString("This may take a few minutes", bodyW() / 2, bodyH() / 2 + 8);
    } else {
      sp.drawString("Waiting for GPS signal...", bodyW() / 2, bodyH() / 2 - 12);
      sp.setTextColor(TFT_DARKGREY);
      sp.drawString("Go outside for best reception", bodyW() / 2, bodyH() / 2 + 4);
    }
    sp.setTextColor(TFT_YELLOW);
    sp.drawString(anim, bodyW() / 2, bodyH() / 2 + 24);
    sp.pushSprite(bodyX(), bodyY());
    sp.deleteSprite();
    return;
  }
  if (_state == STATE_INFO) { _renderInfo(); return; }
  if (_state == STATE_WARDRIVING) { _renderWardriver(); return; }
  ListScreen::onRender();
}

void GPSScreen::onBack() {
  if (_state == STATE_MENU) {
    ShowStatusAction::show("Stopping GPS...", 0);
    _gps.end();
    _disableGnssPower();
    Screen.setScreen(new ModuleMenuScreen());
  } else {
    _showMenu();
  }
}

void GPSScreen::onItemSelected(uint8_t index) {
  if (_state == STATE_MENU) {
    switch (index) {
      case 0:
        _state = STATE_INFO;
        _renderInfo();
        break;
      case 1: {
        if (!_gps.initWardrive(Uni.Storage)) {
          ShowStatusAction::show(("Wardrive error:\n" + _gps.wardriveError()).c_str());
          render();
          return;
        }
        _wardLog.clear();
        _wardLog.addLine(("File: " + _gps.wardriveFilename()).c_str(), TFT_DARKGREY);
        _state = STATE_WARDRIVING;
        _renderWardriver();
        break;
      }
    }
  }
}

void GPSScreen::_showMenu() {
  _state = STATE_MENU;
  _infoInitialized = false;
  setItems(_menuItems);
}

void GPSScreen::_renderInfo() {
  _lastRender = millis();

  auto& g = _gps.gps;
  _infoRows[0] = {"LAT", String(g.location.lat(), 9)};
  _infoRows[1] = {"LNG", String(g.location.lng(), 9)};
  _infoRows[2] = {"Speed", String(g.speed.kmph(), 2) + " km/h"};
  _infoRows[3] = {"Course", String(g.course.deg(), 2) + " deg"};
  _infoRows[4] = {"Altitude", String(g.altitude.meters(), 2) + " m"};
  _infoRows[5] = {"Satellites", String(g.satellites.value())};
  _infoRows[6] = {"Date", _gps.getCurrentDate()};
  _infoRows[7] = {"Time", _gps.getCurrentTime() + " UTC"};

  if (!_infoInitialized) {
    _infoView.setRows(_infoRows, 8);
    _infoInitialized = true;
  }
  _infoView.render(bodyX(), bodyY(), bodyW(), bodyH());
}

void GPSScreen::_wardStatusCb(TFT_eSprite& sp, int barY, int width, void* userData) {
  auto* self = (GPSScreen*)userData;

  sp.setTextDatum(TL_DATUM);
  sp.setTextColor(TFT_GREEN);

  float dist = self->_gps.totalDistance();
  char left[40];
  if (dist >= 1000)
    snprintf(left, sizeof(left), "W:%u B:%u %.1fkm",
             self->_gps.discoveredCount(), self->_gps.bleDiscoveredCount(), dist / 1000.0f);
  else
    snprintf(left, sizeof(left), "W:%u B:%u %dm",
             self->_gps.discoveredCount(), self->_gps.bleDiscoveredCount(), (int)dist);
  sp.drawString(left, 2, barY);

  sp.setTextDatum(TR_DATUM);
  unsigned long rt = self->_gps.wardriveRuntime() / 1000;
  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
           (int)(rt / 3600), (int)((rt % 3600) / 60), (int)(rt % 60));
  sp.drawString(timeBuf, width - 2, barY);
}

void GPSScreen::_renderWardriver() {
  _lastRender = millis();

  // Poll recent finds and add to log
  GPSModule::FoundEntry finds[10];
  uint8_t count = _gps.getRecentFinds(finds, 10);
  if (count > 0 && _wardLog.count() == 1 && _gps.discoveredCount() + _gps.bleDiscoveredCount() <= count) {
    _wardLog.clear();
  }
  unsigned long secs = _gps.wardriveRuntime() / 1000;
  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu:%02lu", secs / 3600, (secs / 60) % 60, secs % 60);
  for (uint8_t i = 0; i < count; i++) {
    auto& f = finds[i];
    char line[60];
    if (f.isBle)
      snprintf(line, sizeof(line), "[+] %s [BLE] %s %s", timeBuf, f.name, f.addr);
    else
      snprintf(line, sizeof(line), "[+] %s %s %s", timeBuf, f.name, f.addr);
    _wardLog.addLine(line, f.isBle ? TFT_CYAN : TFT_WHITE);
  }

  _wardLog.draw(Uni.Lcd, bodyX(), bodyY(), bodyW(), bodyH(), _wardStatusCb, this);
}


void GPSScreen::_enableGnssPower() {
#ifdef EXPANDS_GNSS_EN
  static constexpr uint8_t XL9555_ADDR = 0x20;

  // Read current port 0 direction and output to preserve other bits
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x06);
  Wire.endTransmission(false);
  Wire.requestFrom(XL9555_ADDR, (uint8_t)1);
  uint8_t currentDir = Wire.available() ? Wire.read() : 0xFF;

  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x02);
  Wire.endTransmission(false);
  Wire.requestFrom(XL9555_ADDR, (uint8_t)1);
  uint8_t currentOut = Wire.available() ? Wire.read() : 0;

  // Configure GNSS_EN and GNSS_RST as outputs
  uint8_t outMask = (1u << EXPANDS_GNSS_EN);
#ifdef EXPANDS_GNSS_RST
  outMask |= (1u << EXPANDS_GNSS_RST);
#endif
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x06);
  Wire.write(currentDir & ~outMask);
  Wire.endTransmission();

  // Power on with reset asserted (LOW)
  uint8_t outVal = currentOut | (1u << EXPANDS_GNSS_EN);
#ifdef EXPANDS_GNSS_RST
  outVal &= ~(1u << EXPANDS_GNSS_RST);  // LOW = assert reset during power-up
#endif
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x02);
  Wire.write(outVal);
  Wire.endTransmission();

  delay(200);  // let power stabilize

#ifdef EXPANDS_GNSS_RST
  // Release reset (HIGH = normal operation)
  outVal |= (1u << EXPANDS_GNSS_RST);
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x02);
  Wire.write(outVal);
  Wire.endTransmission();

  delay(500);  // let GNSS module initialize after reset release
#endif
#endif
}

void GPSScreen::_disableGnssPower() {
#ifdef EXPANDS_GNSS_EN
  static constexpr uint8_t XL9555_ADDR = 0x20;

  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x02);
  Wire.endTransmission(false);
  Wire.requestFrom(XL9555_ADDR, (uint8_t)1);
  uint8_t currentOut = Wire.available() ? Wire.read() : 0;

  // Assert reset first, then cut power
  uint8_t clearMask = (1u << EXPANDS_GNSS_EN);
#ifdef EXPANDS_GNSS_RST
  clearMask |= (1u << EXPANDS_GNSS_RST);  // LOW = assert reset
#endif
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(0x02);
  Wire.write(currentOut & ~clearMask);
  Wire.endTransmission();
#endif
}
