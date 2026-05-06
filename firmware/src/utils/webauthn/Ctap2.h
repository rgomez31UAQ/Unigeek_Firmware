#pragma once

#include <stdint.h>
#include <stddef.h>

namespace webauthn {

// Ctap2 — handles CTAP2 (CBOR-encoded) authenticator commands. Plug into
// Ctaphid via setHandler() with `Ctap2::dispatch` as the callback.
//
// Commands implemented:
//   0x01 authenticatorMakeCredential         (rk=true → resident/discoverable creds)
//   0x02 authenticatorGetAssertion           (empty allowList → resident cred lookup)
//   0x04 authenticatorGetInfo
//   0x06 authenticatorClientPIN             (proto v1, subcommands 1-5 + 9)
//   0x07 authenticatorReset
//   0x08 authenticatorGetNextAssertion      (multi-cred discoverable signin)
//   0x0A authenticatorCredentialManagement  (CTAP 2.1 §6.8, subcommands 1-6)
//   0x0B authenticatorSelection             (CTAP 2.1 §6.9)
//   0x0D authenticatorConfig                (CTAP 2.1 §6.11, subcmds 0x02 toggleAlwaysUv + 0x03 setMinPINLength)
//
// Commands stubbed:
//   (none)
//
// User presence (UP): wired to a user-presence callback installed by the
// WebAuthn screen via setUserPresenceFn(); auto-asserted when callback is null.
class Ctap2 {
public:
  using UserPresenceFn = bool (*)(const char* rpId, void* user);

  // Dispatch entry point — matches Ctaphid::HandlerFn signature exactly.
  static uint16_t dispatch(uint8_t  cmd,
                           const uint8_t* req, uint16_t reqLen,
                           uint8_t* resp,      uint16_t respMax,
                           uint8_t* respCmd,
                           void*    user);

  // Optional: install a user-presence prompt. If unset, UP is auto-asserted.
  static void setUserPresenceFn(UserPresenceFn fn, void* user = nullptr);

  // (Re)generate the per-power-cycle pinUvAuthToken (16 random bytes for
  // proto v1) and clear the per-boot bad-PIN counter. Call from
  // WebAuthnScreen::onInit each time the screen opens (= start of a new
  // USB session, which is the closest UniGeek proxy for CTAP "power cycle").
  static void initPinAuthToken();

private:
  // Per-command handlers. Each writes a CTAP2 response (status byte + CBOR
  // payload) into `out`/`outMax`, returning the total length, and sets
  // `*outStatus` to the leading status byte (which is byte 0 of the
  // response). Returning 0 indicates only the status byte is written.
  static uint16_t _handleGetInfo      (const uint8_t* req, uint16_t reqLen,
                                       uint8_t* out, uint16_t outMax);
  static uint16_t _handleMakeCredential(const uint8_t* req, uint16_t reqLen,
                                        uint8_t* out, uint16_t outMax);
  static uint16_t _handleGetAssertion (const uint8_t* req, uint16_t reqLen,
                                       uint8_t* out, uint16_t outMax);
  static uint16_t _handleGetNextAssertion(uint8_t* out, uint16_t outMax);
  static uint16_t _handleReset        (uint8_t* out, uint16_t outMax);
  static uint16_t _handleClientPin    (const uint8_t* req, uint16_t reqLen,
                                       uint8_t* out, uint16_t outMax);
  static uint16_t _handleCredentialManagement(const uint8_t* req, uint16_t reqLen,
                                              uint8_t* out, uint16_t outMax);
  static uint16_t _handleAuthenticatorConfig (const uint8_t* req, uint16_t reqLen,
                                              uint8_t* out, uint16_t outMax);
};

}  // namespace webauthn
