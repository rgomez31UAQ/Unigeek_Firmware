#pragma once

#include <Arduino.h>  // pulls pins_arduino.h so DEVICE_HAS_WEBAUTHN is defined

#ifdef DEVICE_HAS_WEBAUTHN

#include "ui/templates/ListScreen.h"

// Hub for on-device WebAuthn management. Pushes either the BIP-39 backup
// screen or the resident-credential list. Does NOT touch USB FIDO HID;
// safe to enter while a host is not connected.
class WebAuthnManageScreen : public ListScreen {
public:
  const char* title() override { return "Manage WebAuthn"; }

  void onInit()                       override;
  void onItemSelected(uint8_t index)  override;

private:
  ListItem _items[2] = {
    {"BIP39 Backup"},
    {"Passkeys"},
  };
};

#endif  // DEVICE_HAS_WEBAUTHN
