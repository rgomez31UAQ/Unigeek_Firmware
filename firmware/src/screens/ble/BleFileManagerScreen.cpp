#include "BleFileManagerScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/INavigation.h"
#include "utils/uart/BleFileManager.h"

static const char* kBleName = "UniGeek FM";
static const char* kUrl     = "unigeek.xid.run/app/files/";

BleFileManagerScreen::~BleFileManagerScreen() {
  BleFM.end();
}

void BleFileManagerScreen::onInit() {
  BleFM.begin(kBleName);
  _lastConnected = false;
  _firstRender   = true;
}

void BleFileManagerScreen::onUpdate() {
  BleFM.update();

  // BaseScreen doesn't poll Nav — handle BACK ourselves so the user can leave
  // the screen (which calls our destructor → BleFM.end()).
  if (Uni.Nav && Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK) {
      Screen.goBack();
      return;
    }
  }

  bool nowConnected = BleFM.isConnected();
  if (nowConnected != _lastConnected) {
    _lastConnected = nowConnected;
    render();
  }
}

void BleFileManagerScreen::onRender() {
  auto& lcd = Uni.Lcd;
  uint16_t bx = bodyX();
  uint16_t by = bodyY();
  uint16_t bw = bodyW();
  uint16_t bh = bodyH();

  lcd.fillRect(bx, by, bw, bh, TFT_BLACK);
  lcd.setTextFont(1);
  lcd.setTextSize(1);

  bool connected = BleFM.isConnected();

  // ── Top row: BLE name (left) · status (right) ───────────────────────────
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.drawString(kBleName, bx + 2, by + 2);

  lcd.setTextDatum(TR_DATUM);
  if (connected) {
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.drawString("Connected", bx + bw - 2, by + 2);
  } else {
    lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
    lcd.drawString("Advertising", bx + bw - 2, by + 2);
  }

  // ── Centered URL (mirrors WebFileManagerScreen's _drawRunning layout) ──
  int cx   = bx + bw / 2;
  int midY = by + bh / 2;
  lcd.setTextDatum(TC_DATUM);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.drawString(kUrl, cx, midY - 4);

  // ── Footer hint ─────────────────────────────────────────────────────────
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.drawString("BACK to stop", cx, by + bh - 14);

  _firstRender = false;
}
