#include "Bip39.h"

#include <mbedtls/sha256.h>
#include <string.h>

namespace unigeek {
namespace crypto {

size_t Bip39::wordCountForEntropy(size_t entropyLen)
{
  switch (entropyLen) {
    case 16: return 12;
    case 20: return 15;
    case 24: return 18;
    case 28: return 21;
    case 32: return 24;
    default: return 0;
  }
}

bool Bip39::encode(const uint8_t* entropy, size_t entropyLen, uint16_t* outIdx)
{
  if (!entropy || !outIdx) return false;
  size_t words = wordCountForEntropy(entropyLen);
  if (!words) return false;

  // Checksum = first (entropyLen / 4) bits of SHA-256(entropy).
  uint8_t hash[32];
  if (mbedtls_sha256_ret(entropy, entropyLen, hash, 0) != 0) return false;
  size_t csBits = entropyLen / 4;        // 4..8 bits

  // Concatenate entropy || checksum into a bit buffer (max 264 bits = 33 B).
  uint8_t buf[33] = {0};
  memcpy(buf, entropy, entropyLen);
  // Append the top csBits of hash[0] right after the entropy.
  // Since csBits <= 8, hash[0]'s high csBits are what we want.
  // Bits live MSB-first in this scheme.
  uint8_t csByte = (uint8_t)(hash[0] & (0xFF << (8 - csBits)));
  buf[entropyLen] = csByte;

  // Slice into 11-bit chunks, MSB-first.
  for (size_t w = 0; w < words; w++) {
    size_t bitPos = w * 11;
    size_t bytePos = bitPos / 8;
    size_t bitInByte = bitPos % 8;

    // Read 24 bits starting at bytePos, then shift.
    uint32_t v = ((uint32_t)buf[bytePos]     << 16)
               | ((uint32_t)buf[bytePos + 1] <<  8)
               | ((uint32_t)buf[bytePos + 2]);
    uint32_t shifted = v >> (24 - bitInByte - 11);
    outIdx[w] = (uint16_t)(shifted & 0x07FF);  // 11-bit mask
  }
  return true;
}

}  // namespace crypto
}  // namespace unigeek
