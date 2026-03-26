#pragma once

#include <MFRC522_I2C.h>
#include <cstdint>

namespace NestedAttack {

struct Result {
  bool success = false;
  uint64_t key = 0;
};

// Return false from callback to cancel the attack
using ProgressFn = bool(*)(const char* msg, int pct);

/**
 * Perform a nested attack to recover an unknown sector key.
 *
 * @param module       MFRC522_I2C instance (already initialized, antenna on)
 * @param uid          32-bit card UID
 * @param authCmd      PICC_CMD_MF_AUTH_KEY_A (0x60) or KEY_B (0x61)
 * @param exploitBlock trailer block of sector with known key
 * @param knownKey     48-bit known key for the exploit sector
 * @param targetBlock  trailer block of sector to crack
 * @param progress     optional progress callback
 */
Result crack(MFRC522_I2C* module, uint32_t uid,
             uint8_t authCmd, uint8_t exploitBlock, uint64_t knownKey,
             uint8_t targetBlock, ProgressFn progress = nullptr);

}
