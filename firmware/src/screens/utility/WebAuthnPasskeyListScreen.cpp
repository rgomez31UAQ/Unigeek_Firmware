#include "WebAuthnPasskeyListScreen.h"

#ifdef DEVICE_HAS_WEBAUTHN

#include "core/Device.h"
#include "core/ConfigManager.h"
#include "ui/actions/InputSelectAction.h"
#include "ui/actions/ShowStatusAction.h"

#include <string.h>

void WebAuthnPasskeyListScreen::_enumCb(
    const webauthn::CredentialStore::ResidentCredRecord& rec, void* ctx)
{
  auto* self = static_cast<WebAuthnPasskeyListScreen*>(ctx);
  if (self->_count >= kMaxEntries) return;
  uint8_t i = self->_count;
  self->_entries[i] = rec;
  self->_items[i].label    = self->_entries[i].rpId;
  self->_items[i].sublabel = self->_entries[i].userName[0]
                             ? self->_entries[i].userName : nullptr;
  self->_count++;
}

void WebAuthnPasskeyListScreen::_reload(uint8_t selectedIdx)
{
  _count = 0;
  webauthn::CredentialStore::init();
  webauthn::CredentialStore::enumAllResidentCreds(&_enumCb, this);
  uint8_t sel = selectedIdx;
  if (_count > 0 && sel >= _count) sel = _count - 1;
  setItems(_items, _count, sel);
}

void WebAuthnPasskeyListScreen::onInit()
{
  _reload();
}

void WebAuthnPasskeyListScreen::onRender()
{
  if (_count == 0) {
    auto& lcd = Uni.Lcd;
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.drawString("No passkeys saved",
                   bodyX() + bodyW() / 2, bodyY() + bodyH() / 2);
    return;
  }
  ListScreen::onRender();
}

void WebAuthnPasskeyListScreen::onItemSelected(uint8_t index)
{
  if (index >= _count) return;
  _confirmDelete(index);
}

void WebAuthnPasskeyListScreen::_confirmDelete(uint8_t index)
{
  static constexpr InputSelectAction::Option opts[] = {
    {"Cancel", "cancel"},
    {"Delete", "delete"},
  };
  const char* sel = InputSelectAction::popup(_entries[index].rpId, opts, 2, "cancel");
  if (!sel || strcmp(sel, "delete") != 0) {
    render();
    return;
  }

  // Capture credId before reload (which overwrites _entries).
  uint8_t credId[webauthn::CredentialStore::kCredIdSize];
  memcpy(credId, _entries[index].credId, sizeof(credId));

  if (!webauthn::CredentialStore::deleteResidentCredById(credId)) {
    ShowStatusAction::show("Delete failed", 1200);
    render();
    return;
  }
  ShowStatusAction::show("Deleted", 800);
  _reload(index);
}

#endif  // DEVICE_HAS_WEBAUTHN
