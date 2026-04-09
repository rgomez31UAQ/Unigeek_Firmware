#include "screens/CharacterScreen.h"

#include "core/AchievementManager.h"
#include "core/ConfigManager.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "screens/MainMenuScreen.h"

// ─── helpers ─────────────────────────────────────────────────────────────────
namespace {

int _clampPct(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

// Bar whose label and value are drawn INSIDE the filled region.
//   label  — left-aligned inside bar (e.g. "HP", "BRAIN")
//   value  — right-aligned inside bar (e.g. "85%", "24/122")
//   pct    — 0-100 fill level
//   fillColor — colour for the filled portion
void _drawInlineBar(TFT_eSprite& sp, int x, int y, int w, int h,
                    const char* label, const char* value,
                    int pct, uint16_t fillColor, int scale = 1)
{
  const uint16_t kEmptyBg = 0x2104;
  pct = _clampPct(pct);

  sp.fillRect(x, y, w, h, kEmptyBg);
  sp.drawRect(x, y, w, h, TFT_DARKGREY);

  int fill = (w - 2) * pct / 100;
  if (fill > 0) sp.fillRect(x + 1, y + 1, fill, h - 2, fillColor);

  // font 1 has 1 px dead row at the bottom — shift up by 1 to visually centre
  int ty = y + (h - scale * 8) / 2 + 1;

  sp.setTextDatum(TL_DATUM);
  sp.setTextColor(TFT_WHITE);
  sp.drawString(label, x + 5, ty, 1);

  sp.setTextDatum(TR_DATUM);
  sp.setTextColor(TFT_WHITE);
  sp.drawString(value, x + w - 5, ty, 1);
}

struct RankInfo { const char* label; uint16_t color; };

RankInfo _getRank(int exp)
{
  if (exp >= 10000) return { "LEGEND", TFT_VIOLET   };
  if (exp >= 5000)  return { "ELITE",  TFT_YELLOW   };
  if (exp >= 2000)  return { "EXPERT", TFT_CYAN     };
  if (exp >= 500)   return { "HACKER", TFT_GREEN    };
                    return { "NOVICE", TFT_DARKGREY };
}

int _domainUnlocked(const AchievementManager::AchDef* cat, uint8_t domain)
{
  int n = 0;
  for (int i = 0; i < AchievementManager::kAchCount; i++)
    if (cat[i].domain == domain && Achievement.isUnlocked(cat[i].id)) n++;
  return n;
}

int _domainTotal(const AchievementManager::AchDef* cat, uint8_t domain)
{
  int n = 0;
  for (int i = 0; i < AchievementManager::kAchCount; i++)
    if (cat[i].domain == domain) n++;
  return n;
}

} // namespace

// ─── CharacterScreen ─────────────────────────────────────────────────────────

// Full-screen exception: skip BaseScreen chrome so we own every pixel.
void CharacterScreen::update()
{
  onUpdate();
  Achievement.drawToastIfNeeded(0, 0, Uni.Lcd.width(), Uni.Lcd.height());
}

void CharacterScreen::render()
{
  if (Uni.lcdOff) return;
  onRender();
}

void CharacterScreen::onInit()
{
  _lastRefreshMs = 0;
}

void CharacterScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_PRESS || dir == INavigation::DIR_BACK) {
      _enterMainMenu();
      return;
    }
  }

  unsigned long now = millis();
  if (now - _lastRefreshMs > 2000) {
    _lastRefreshMs = now;
    render();
  }
}

void CharacterScreen::onRender()
{
  TFT_eSprite sp(&Uni.Lcd);
  const int W = Uni.Lcd.width();
  const int H = Uni.Lcd.height();
  sp.createSprite(W, H);
  sp.fillSprite(TFT_BLACK);

  const int      PAD   = 4;
  const uint16_t theme = Config.getThemeColor();

  // ── screen scale (base: Cardputer = 240 px wide) ──────────────────────────
  // Always use font 1 + setTextSize(scale) — font 1 is always compiled in,
  // unlike font 2/4 which require LOAD_FONT2/4 in User_Setup.h.
  const int scale   = W < 360 ? 1 : W < 600 ? 2 : 3;
  const int lineH   = scale * 8;
  const int barH    = scale * 16;
  const int domBarH = scale * 13;
  const int gap     = scale * 2;   // single spacing unit — used everywhere

  sp.setTextSize(scale);  // scale font 1 up for larger screens

  // ── collect data ──────────────────────────────────────────────────────────
  const auto* cat    = AchievementManager::catalog();
  const int   kTotal = (int)AchievementManager::kAchCount;

  int numUnlk = 0;
  for (int i = 0; i < kTotal; i++)
    if (Achievement.isUnlocked(cat[i].id)) numUnlk++;

  int      exp   = Achievement.getExp();
  RankInfo rank  = _getRank(exp);
  int      hp    = _clampPct(Uni.Power.getBatteryPercentage());
  bool     chg   = Uni.Power.isCharging();
  if (hp == 0 && !chg) hp = 100;
  int      brain = ESP.getHeapSize() > 0
                 ? _clampPct((ESP.getFreeHeap() * 100) / ESP.getHeapSize()) : 0;
  String   agent = Config.get(APP_CONFIG_DEVICE_NAME, APP_CONFIG_DEVICE_NAME_DEFAULT);

  int cx = PAD;
  int cy = PAD + 2;

  const int indent = sp.textWidth("AGENT ", 1);

  // ── SECTION 1: identity ───────────────────────────────────────────────────
  sp.setTextDatum(TL_DATUM);
  sp.setTextColor(TFT_DARKGREY);
  sp.drawString("AGENT", cx, cy, 1);
  sp.setTextColor(TFT_WHITE);
  sp.drawString(agent.substring(0, 15).c_str(), cx + indent, cy, 1);
  sp.setTextDatum(TR_DATUM);
  sp.setTextColor(rank.color);
  sp.drawString(rank.label, W - PAD, cy, 1);
  cy += lineH + gap;

  {
    char expBuf[12];
    snprintf(expBuf, sizeof(expBuf), "%d", exp);
    sp.setTextDatum(TL_DATUM);
    sp.setTextColor(TFT_DARKGREY);
    sp.drawString("EXP", cx, cy, 1);
    sp.setTextColor(TFT_ORANGE);
    sp.drawString(expBuf, cx + indent, cy, 1);

    int nextExp = (exp < 500)  ? 500  : (exp < 2000) ? 2000
                : (exp < 5000) ? 5000 : 10000;
    int prevExp = (exp < 500)  ? 0    : (exp < 2000) ? 500
                : (exp < 5000) ? 2000 : 5000;
    int rPct    = (exp >= 10000) ? 100
                : _clampPct((exp - prevExp) * 100 / (nextExp - prevExp));

    int bx     = W * 5 / 8;
    int bw     = W - bx - PAD;
    int rBarH  = scale * 6;
    sp.drawRect(bx, cy + scale, bw, rBarH, TFT_DARKGREY);
    int fill = (bw - 2) * rPct / 100;
    if (fill > 0) sp.fillRect(bx + 1, cy + scale + 1, fill, rBarH - 2, theme);
  }
  cy += lineH + gap;

  // ── SECTION 2: vitals ─────────────────────────────────────────────────────
  const int halfW = (W - PAD * 2 - gap) / 2;  // two bars + gap

  // Row 1: HP | BRAIN side by side
  char hpBuf[8];
  snprintf(hpBuf, sizeof(hpBuf), "%d%%", hp);
  _drawInlineBar(sp, cx, cy, halfW, barH,
                 chg ? "HP +CHG" : "HP", hpBuf, hp, TFT_RED, scale);

  char brainBuf[8];
  snprintf(brainBuf, sizeof(brainBuf), "%d%%", brain);
  _drawInlineBar(sp, cx + halfW + gap, cy, halfW, barH,
                 "BRAIN", brainBuf, brain, TFT_DARKGREEN, scale);
  cy += barH + gap;

  // Row 2: ACHIEVEMENT full-width
  char achBuf[16];
  int achPct = kTotal > 0 ? (numUnlk * 100 / kTotal) : 0;
  snprintf(achBuf, sizeof(achBuf), "%d/%d", numUnlk, kTotal);
  _drawInlineBar(sp, cx, cy, W - PAD * 2, barH,
                 "ACHIEVEMENT", achBuf, achPct, TFT_ORANGE, scale);
  cy += barH + gap;

  // ── SECTION 3: domain inline bars (4 cols × 2 rows) ──────────────────────
  struct DomEntry { const char* abbr; uint8_t dom; uint16_t col; };
  const DomEntry kDoms[8] = {
    { "WiFi", 0, TFT_CYAN    },
    { "Atk",  1, TFT_RED     },
    { "BT",   2, TFT_BLUE    },
    { "HID",  3, TFT_YELLOW  },
    { "NFC",  4, TFT_GREEN   },
    { "IR",   5, TFT_ORANGE  },
    { "RF",   6, TFT_MAGENTA },
    { "GPS",  7, TFT_WHITE   },
  };
  const int kCols    = 4;
  const int domBarW  = (W - PAD * 2 - (kCols - 1) * gap) / kCols;
  const int domStep  = domBarW + gap;

  for (int i = 0; i < 8; i++) {
    int col = i % kCols;
    int row = i / kCols;
    int dx  = cx + col * domStep;
    int dy  = cy + row * (domBarH + gap);

    int tot = _domainTotal(cat, kDoms[i].dom);
    int unl = _domainUnlocked(cat, kDoms[i].dom);
    int pct = (tot > 0) ? (unl * 100 / tot) : 0;

    // last column stretches to W-PAD so right edge aligns with section 2
    int bw = (col == kCols - 1) ? (W - PAD - dx) : domBarW;
    _drawInlineBar(sp, dx, dy, bw, domBarH, kDoms[i].abbr, "", pct, kDoms[i].col, scale);
  }
  cy += domBarH * 2 + gap + gap;

  // ── footer ────────────────────────────────────────────────────────────────
  sp.setTextDatum(BC_DATUM);
  sp.setTextColor(TFT_DARKGREY);
#ifdef DEVICE_HAS_KEYBOARD
  sp.drawString("ENTER / BACK: Main Menu", W / 2, H - 2, 1);
#else
  sp.drawString("Press: Main Menu", W / 2, H - 2, 1);
#endif

  sp.pushSprite(0, 0);
  sp.deleteSprite();
}

void CharacterScreen::_enterMainMenu()
{
  Screen.setScreen(new MainMenuScreen());
}
