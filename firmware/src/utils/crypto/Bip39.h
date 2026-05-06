#pragma once

#include <stdint.h>
#include <stddef.h>

namespace unigeek {
namespace crypto {

// BIP-39 mnemonic encoding (English wordlist only).
//
// Standard mappings:
//   16 B entropy -> 12 words (CS=4 bits)
//   20 B         -> 15 words (CS=5 bits)
//   24 B         -> 18 words (CS=6 bits)
//   28 B         -> 21 words (CS=7 bits)
//   32 B entropy -> 24 words (CS=8 bits)   <-- used for FIDO master key backup
//
// Encoder takes the entropy bytes and returns indices into the BIP-39 English
// wordlist, NOT the words themselves; callers look up words via
// `kBip39EnglishWordlist[idx]` to keep this module Arduino-free / testable.
class Bip39 {
public:
  // Returns the number of words for `entropyLen` bytes, or 0 if invalid.
  // Valid entropy lengths: 16, 20, 24, 28, 32.
  static size_t wordCountForEntropy(size_t entropyLen);

  // Encode `entropy` into BIP-39 word indices. `outIdx` must hold at least
  // wordCountForEntropy(entropyLen) entries. Returns true on success.
  // Computes the SHA-256-based checksum internally; caller does not provide it.
  static bool encode(const uint8_t* entropy, size_t entropyLen,
                     uint16_t* outIdx);
};

}  // namespace crypto
}  // namespace unigeek
