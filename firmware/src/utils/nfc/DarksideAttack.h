#pragma once

#include <MFRC522_I2C.h>
#include <cstdint>
#include "core/IStorage.h"

namespace DarksideAttack {

struct Result {
  bool success = false;
  uint64_t key = 0;
};

using ProgressFn = bool(*)(const char* msg, int pct);

/**
 * Darkside attack — recovers a key when NO key is known.
 *
 * Collects encrypted parity oracle data from failed auth attempts,
 * then fast-verifies candidate keys via software CRYPTO1 simulation.
 * Keys sourced from built-in common keys + dictionary files on storage.
 *
 * @param module       MFRC522_I2C instance
 * @param uid          32-bit card UID
 * @param authCmd      PICC_CMD_MF_AUTH_KEY_A or KEY_B
 * @param blockAddr    trailer block to attack
 * @param storage      storage for dictionary files (nullable)
 * @param progress     optional progress callback
 */
Result crack(MFRC522_I2C* module, uint32_t uid,
             uint8_t authCmd, uint8_t blockAddr,
             IStorage* storage, ProgressFn progress = nullptr);

}
