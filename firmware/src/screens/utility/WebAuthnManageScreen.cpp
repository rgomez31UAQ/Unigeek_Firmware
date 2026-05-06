#include "WebAuthnManageScreen.h"

#ifdef DEVICE_HAS_WEBAUTHN

#include "core/ScreenManager.h"
#include "screens/utility/WebAuthnBackupScreen.h"
#include "screens/utility/WebAuthnPasskeyListScreen.h"

void WebAuthnManageScreen::onInit()
{
  setItems(_items);
}

void WebAuthnManageScreen::onItemSelected(uint8_t index)
{
  switch (index) {
    case 0: Screen.push(new WebAuthnBackupScreen());      break;
    case 1: Screen.push(new WebAuthnPasskeyListScreen()); break;
  }
}

#endif  // DEVICE_HAS_WEBAUTHN
