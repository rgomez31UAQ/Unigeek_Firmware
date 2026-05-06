#pragma once

#include <Arduino.h>  // pulls pins_arduino.h so DEVICE_HAS_WEBAUTHN is defined

#ifdef DEVICE_HAS_WEBAUTHN

#include "ui/templates/ListScreen.h"
#include "utils/webauthn/CredentialStore.h"

// On-device passkey manager: lists every resident credential and lets the
// user delete one. Reads + writes /unigeek/utility/fido/credentials/ via
// CredentialStore. Does NOT touch USB FIDO HID — safe to enter while a
// host is not connected.
class WebAuthnPasskeyListScreen : public ListScreen {
public:
  const char* title() override { return "Passkeys"; }

  void onInit()                       override;
  void onRender()                     override;
  void onItemSelected(uint8_t index)  override;

private:
  static constexpr uint8_t kMaxEntries = 16;

  webauthn::CredentialStore::ResidentCredRecord _entries[kMaxEntries];
  ListItem _items[kMaxEntries];
  uint8_t  _count = 0;

  void _reload(uint8_t selectedIdx = 0);
  void _confirmDelete(uint8_t index);

  static void _enumCb(const webauthn::CredentialStore::ResidentCredRecord& rec, void* ctx);
};

#endif  // DEVICE_HAS_WEBAUTHN
