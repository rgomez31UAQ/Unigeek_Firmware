#include "screens/setting/AboutScreen.h"
#include "screens/setting/SettingScreen.h"
#include "core/ScreenManager.h"
#include "core/Device.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

void AboutScreen::onInit()
{
  uint8_t i = 0;

  // ── Firmware info ────────────────────────────────────────────────────────────
  _rows[i++] = { "--- About ---", ""                 };
  _rows[i++] = { "Firmware",  "UniGeek"              };
  _rows[i++] = { "Version",   FIRMWARE_VERSION       };
  _rows[i++] = { "Author",    "L Shaf"               };
  _rows[i++] = { "Platform",  "ESP32 Arduino"        };
  _rows[i++] = { "Framework", "PlatformIO"};

  // ── Thanks To ────────────────────────────────────────────────────────────────
  _rows[i++] = { "--- Thanks To ---", ""                };
  _rows[i++] = { "7h30th3r0n3",       "Evil-M5Project" };
  _rows[i++] = { "pr3y",              "Bruce"           };
  _rows[i++] = { "Flipper-XFW",       "Flipper-IRDB"   };
  _rows[i++] = { "pivotchip",         "FrostedFastPair" };
  _rows[i++] = { "Xinyuan-LilyGO",    "LilyGoLib"      };
  _rows[i++] = { "m5stack",           "M5Unified"      };

  _rowCount = i;
  _view.setRows(_rows, _rowCount);
}

void AboutScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      Screen.setScreen(new SettingScreen());
      return;
    }
    if (_view.onNav(dir)) render();
  }
}

void AboutScreen::onRender()
{
  _view.render(bodyX(), bodyY(), bodyW(), bodyH());
}