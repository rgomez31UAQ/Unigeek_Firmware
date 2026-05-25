// cpp
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>
extern "C" {
  #include "mbedtls/platform.h"
}

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/ConfigManager.h"
#include "core/PinConfigManager.h"
#include "core/AchievementStorage.h"
#include "core/AchievementManager.h"
#include "core/RtcManager.h"
#include "core/RandomSeed.h"
#include "utils/uart/UartFileManager.h"

#include "screens/CharacterScreen.h"
#ifdef DEVICE_HAS_TOUCH_NAV
#include "screens/setting/TouchGuideScreen.h"
#endif
#ifdef DEVICE_CYD
#include "screens/setting/CYDTouchCalScreen.h"
#endif

void _bootSplash() {
  // ── Pre-draw init (config needed for theme colour) ────────────────────────
  Config.load(Uni.Storage);
  PinConfig.load(Uni.Storage);
  Uni.onPinConfigApply();
#ifdef DEVICE_CYD
  if (Uni.Nav)
    Uni.Nav->setTouchSwapXY(Config.get(APP_CONFIG_TOUCH_SWAP_XY, APP_CONFIG_TOUCH_SWAP_XY_DEFAULT) == "1");
#endif
  Uni.applyOrientation();   // rotate screen + flip nav before any drawing
  Uni.Lcd.setBrightness((uint8_t)Config.get(APP_CONFIG_BRIGHTNESS, APP_CONFIG_BRIGHTNESS_DEFAULT).toInt());

  // ── Static UI ─────────────────────────────────────────────────────────────
  auto& lcd = Uni.Lcd;
  uint16_t w = lcd.width();
  uint16_t h = lcd.height();

  lcd.fillScreen(TFT_BLACK);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextSize(3);
  lcd.setTextColor(Config.getThemeColor());
  lcd.drawString("UniGeek", w / 2, h / 2 - 14);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_DARKGREY);
  lcd.drawString(__DATE__, w / 2, h / 2 + 10);

  const uint16_t barW  = w / 2;
  const uint16_t barH  = 4;
  const uint16_t barX  = (w - barW) / 2;
  const uint16_t barY  = h / 2 + 28;
  const uint16_t lblY  = barY + barH + 6;
  const uint16_t fillW = barW - 2;
  lcd.drawRoundRect(barX, barY, barW, barH, 1, TFT_DARKGREY);

  // progress(pct, label) — fills bar to pct% and updates status text
  auto progress = [&](uint8_t pct, const char* label) {
    lcd.fillRect(barX + 1, barY + 1, fillW * pct / 100, barH - 2, Config.getThemeColor());
    lcd.fillRect(0, lblY, w, 12, TFT_BLACK);   // clear full label row before redraw
    lcd.setTextDatum(TC_DATUM);                // top-centre: y is the TOP of the text
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.drawString(label, w / 2, lblY + 2);
  };

  progress(25, "Config loaded");
  delay(300);

  // ── Init steps ────────────────────────────────────────────────────────────
  AchStore.load(Uni.Storage);
  progress(50, "Achievements loaded");
  delay(300);

  Achievement.recalibrate(Uni.Storage);
  progress(75, "EXP calibrated");
  delay(300);

  RandomSeed::init();
  Uni.applyNavMode();
  progress(90, "System ready");
  delay(300);

  if (Uni.Speaker) Uni.Speaker->setVolume((uint8_t)Config.get(APP_CONFIG_VOLUME, APP_CONFIG_VOLUME_DEFAULT).toInt());
  if (Uni.Speaker) Uni.Speaker->playWin();
  progress(100, "Starting...");
  delay(300);
}

// ── mbedTLS allocator override (PSRAM boards only) ───────────────────────────
//
// mbedTLS's rx buffer wants a contiguous ~16 KB block. Once the Lua VM (or any
// busy subsystem) has run, internal SRAM is fragmented even though total free
// is plenty — TLS handshake fails with -32512 (MBEDTLS_ERR_SSL_ALLOC_FAILED).
//
// On PSRAM boards (cores3, sticks3) we route every mbedTLS alloc to PSRAM.
// 8 MB of empty external RAM dodges fragmentation entirely; PSRAM cache
// contention with WiFi isn't latency-critical for TLS.
//
// On no-PSRAM boards (cardputer_adv, cardputer, stickcplus_*) there's no
// clean fix: any reservation we make in internal SRAM competes with the
// WiFi driver's own 40 KB DMA-capable allocation, and starving WiFi is
// worse than failing TLS. HTTPS from Lua scripts on no-PSRAM boards may
// fail under heap pressure — see lua-runner.md.

static void* _mbedtlsPsramCalloc(size_t n, size_t size) {
  void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p;
}
static void _mbedtlsPsramFree(void* p) {
  heap_caps_free(p);
}

void setup() {
  // Default 256-byte RX FIFO for now — only grown below if the Serial File
  // Manager is enabled, since that's the only consumer that needs it.
  Serial.begin(115200);

  if (psramFound()) {
    mbedtls_platform_set_calloc_free(_mbedtlsPsramCalloc, _mbedtlsPsramFree);
  }

  Uni.begin();
  Uni.initStorage();
  Config.load(Uni.Storage);   // load early so optional services can be gated

  // Serial File Manager — optional always-on USB service. When disabled it's
  // skipped entirely, reclaiming ~12 KB internal SRAM (8 KB FileManagerCore
  // frame buffer + 4 KB Serial RX FIFO) — meaningful on no-PSRAM boards.
  // UartFileManager streams up to ~1 KB per frame, so the 256-byte default RX
  // FIFO would overflow before loop() can drain it (breaking upload CRCs);
  // grow it to 4 KB only when the service is on. Resizing needs a Serial
  // restart because setRxBufferSize() is ignored once the driver is installed.
  if (Config.get(APP_CONFIG_SERIAL_FM, APP_CONFIG_SERIAL_FM_DEFAULT).toInt()) {
    Serial.end();
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    UartFM.begin();
  }
#ifdef DEVICE_HAS_RTC
  RtcManager::syncSystemFromRtc();
#endif
  _bootSplash();
#ifdef DEVICE_CYD
  if (Config.get(APP_CONFIG_TOUCH_CALIBRATED, APP_CONFIG_TOUCH_CALIBRATED_DEFAULT) == "0")
    Screen.setScreen(new CYDTouchCalScreen());
  else
#endif
#ifdef DEVICE_HAS_TOUCH_NAV
  if (Config.get(APP_CONFIG_TOUCH_GUIDE_SHOWN, APP_CONFIG_TOUCH_GUIDE_SHOWN_DEFAULT) == "0")
    Screen.setScreen(new TouchGuideScreen(false));
  else
#endif
  Screen.setScreen(new CharacterScreen());
}

void loop() {
  Uni.update();
  UartFM.update();

  // ── Power saving ──────────────────────────────────────────────────────────
  IScreen* _cur       = Screen.current();
  bool _psInhibit     = _cur && _cur->inhibitPowerSave();
#ifdef APP_MENU_POWER_OFF
  bool _poInhibit     = _psInhibit || (_cur && _cur->inhibitPowerOff());
#endif

  if (!_psInhibit && Config.get(APP_CONFIG_ENABLE_POWER_SAVING, APP_CONFIG_ENABLE_POWER_SAVING_DEFAULT).toInt()) {
    unsigned long idle   = millis() - Uni.lastActiveMs;
    unsigned long dispMs = (unsigned long)Config.get(APP_CONFIG_INTERVAL_DISPLAY_OFF, APP_CONFIG_INTERVAL_DISPLAY_OFF_DEFAULT).toInt() * 1000UL;

    if (!Uni.lcdOff && idle > dispMs) {
      Uni.Lcd.setBrightness(0);
      Uni.lcdOff = true;
    } else if (Uni.lcdOff && idle <= dispMs) {
      Uni.Lcd.setBrightness((uint8_t)Config.get(APP_CONFIG_BRIGHTNESS, APP_CONFIG_BRIGHTNESS_DEFAULT).toInt());
      Uni.lcdOff = false;
      // The press that woke the screen only turns the display on — it must
      // never propagate as an action. Drop any already-released wake event and
      // suppress the future release of an in-progress press.
      Uni.Nav->suppressCurrentPress();
#ifdef DEVICE_HAS_KEYBOARD
      if (Uni.Keyboard && Uni.Keyboard->available()) Uni.Keyboard->getKey();
#endif
      if (Screen.current()) Screen.current()->render();
      return;
    }

#ifdef APP_MENU_POWER_OFF
    if (!_poInhibit && Uni.lcdOff && Config.get(APP_CONFIG_ENABLE_POWER_OFF, APP_CONFIG_ENABLE_POWER_OFF_DEFAULT).toInt()) {
      unsigned long powerMs = dispMs + (unsigned long)Config.get(APP_CONFIG_INTERVAL_POWER_OFF, APP_CONFIG_INTERVAL_POWER_OFF_DEFAULT).toInt() * 1000UL;
      if (idle > powerMs) {
        Uni.Power.powerOff();
      }
    }
#endif
  } else if (Uni.lcdOff) {
    // auto display off disabled or inhibited while display was sleeping — restore brightness
    Uni.Lcd.setBrightness((uint8_t)Config.get(APP_CONFIG_BRIGHTNESS, APP_CONFIG_BRIGHTNESS_DEFAULT).toInt());
    Uni.lcdOff = false;
  }

  // ── Screen update ─────────────────────────────────────────────────────────
  Screen.update();

  // ── Live touch overlay (touch-nav boards only) ───────────────────────────
#ifdef DEVICE_HAS_TOUCH_NAV
  if (Uni.Nav && Config.get(APP_CONFIG_SHOW_OVERLAY, APP_CONFIG_SHOW_OVERLAY_DEFAULT).toInt())
    Uni.Nav->drawOverlay();
#endif
}
