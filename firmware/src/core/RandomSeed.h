//
// RandomSeed.h — persistent random seeding via NVS
//
// Combines: ESP32 hardware RNG + MAC address + RTC/system time + rolling counter
// Call init() once at boot, call reseed() after GPS fix or NTP sync.
//

#pragma once

#include <esp_system.h>
#include <esp_mac.h>
#include <Preferences.h>
#include <time.h>

namespace RandomSeed {

static uint32_t _prevSeed = 0;

// `extra` is mixed into the chain — pass a caller-supplied seed (e.g. the
// argument to Lua's math.randomseed) so the user's intent is honored while
// still riding on the device's hardware entropy + rolling chain.
static inline uint32_t _buildSeed(uint32_t extra = 0) {
  uint32_t seed = esp_random();

  // Mix in previous seed (rolling chain)
  seed ^= _prevSeed;

  // Mix in caller-supplied seed
  seed ^= extra;

  // Mix in MAC address (unique per device)
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  for (int i = 0; i < 6; i++)
    seed ^= (uint32_t)mac[i] << ((i % 4) * 8);

  // Mix in current time (RTC or system clock)
  time_t now;
  time(&now);
  seed ^= (uint32_t)now;
  seed ^= (uint32_t)micros();

  _prevSeed = seed;
  return seed;
}

static inline void init() {
  // Load persisted seed from NVS
  Preferences prefs;
  prefs.begin("unigeek", false);
  _prevSeed = prefs.getULong("rseed", 0);

  uint32_t seed = _buildSeed();

  prefs.putULong("rseed", seed);
  prefs.end();

  randomSeed(seed);
}

static inline uint32_t reseed(uint32_t extra = 0) {
  uint32_t seed = _buildSeed(extra);
  randomSeed(seed);
  return seed;
}

}  // namespace RandomSeed