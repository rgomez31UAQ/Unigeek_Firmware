# Screen Patterns Reference

## Rendering Rule (Screen-Off Safe)

    onUpdate() runs ALWAYS — even when display is off (Uni.lcdOff == true).
    render() is guarded by Uni.lcdOff in BaseScreen — it early-returns when display is off.

    NEVER call drawing functions directly from onUpdate().
    Instead, change state then call render(). Let onRender() dispatch by state.

    ✗ BAD:  onUpdate() { _drawLog(); }
    ✗ BAD:  onUpdate() { onRender(); }
    ✗ BAD:  onUpdate() { _scrollView.render(...); }
    ✓ GOOD: onUpdate() { render(); }

    For ListScreen subclasses with multiple states, override onRender():
      void MyScreen::onRender() {
        if (_state == STATE_RUNNING) { _drawLog(); return; }
        ListScreen::onRender();  // default list rendering for menu states
      }

    For BaseScreen subclasses, put all drawing in onRender():
      void MyScreen::onRender() { _renderContent(); }

    If a draw helper needs dynamic data (e.g. a status message), store it in a
    member variable first, then call render() — onRender() reads the member.

---

## Back Navigation

    onBack() is ONLY available on ListScreen — BaseScreen has no onBack() virtual method.
    - ListScreen subclasses: override onBack() — called automatically when DIR_BACK is received
    - BaseScreen subclasses: handle DIR_BACK manually in onUpdate():
        if (Uni.Nav->wasPressed() && Uni.Nav->readDirection() == INavigation::DIR_BACK) { ... }
      Never declare onBack() override on a BaseScreen subclass — it will not compile.
    - IMPORTANT: M5StickC default nav NEVER emits DIR_BACK — only DIR_UP/DOWN/PRESS.
      In any custom state that must support "go back" or "stop", handle BOTH:
        if (Uni.Nav->wasPressed()) {
          auto dir = Uni.Nav->readDirection();
          if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) { /* exit */ }
        }
      And render a device-appropriate exit affordance:
        #ifdef DEVICE_HAS_KEYBOARD
          sp.drawString("BACK: Exit", ...);
        #else
          sp.fillRect(0, bodyH() - 16, bodyW(), 16, Config.getThemeColor());
          sp.setTextColor(TFT_WHITE, Config.getThemeColor());
          sp.drawString("< Back", bodyW() / 2, bodyH() - 8, 1);
        #endif

    - hasBackItem() controls whether "< Back" appears — default true
      Only MainMenuScreen overrides it to false; all other screens use the default
    - "< Back" is hidden on keyboard boards and encoder nav (DIR_BACK handles it instead)
    - setItems() resets _selectedIndex and _scrollOffset to 0 then calls render()
      ONLY call on init or when switching item arrays
    - NEVER call setItems() to refresh sublabels — update sublabels directly on the member array, then call render()
    - Implement onBack() in .cpp when it needs to instantiate a parent screen (breaks circular includes)

---

## Action Overlays (all blocking)

    String      InputTextAction::popup("Title")
    String      InputTextAction::popup("Title", "default")
    String      InputTextAction::popup("Title", "default", true)   // numberMode: digits and dot only
    int         InputNumberAction::popup("Title")
    int         InputNumberAction::popup("Title", 0, 100, 50)
    const char* InputSelectAction::popup("Title", opts, count)
    const char* InputSelectAction::popup("Title", opts, count, "default_value")
    void        ShowStatusAction::show("Message")           wait for button/key press, then wipe  (default, durationMs=-1)
    void        ShowStatusAction::show("Message", 0)        show and return immediately, no wipe
    void        ShowStatusAction::show("Message", 1500)     show, block ms, then wipe
    void        ShowQRCodeAction::show("Label", "content")  blocking QR code overlay

    // After any popup, call render() to restore the screen — actions only wipe their overlay area
    // InputSelectAction: DIR_BACK dismisses and returns nullptr
    // InputTextAction numberMode=true: scroll mode shows 0-9 and . only

    // InputSelectAction::Option — always static constexpr inside method
    static constexpr InputSelectAction::Option opts[] = {
      {"Label", "value"},
    };

### Include paths

    #include "ui/actions/InputTextAction.h"
    #include "ui/actions/InputNumberAction.h"
    #include "ui/actions/InputSelectOption.h"   // NOTE: file is InputSelectOption.h not InputSelectAction.h
    #include "ui/actions/ShowStatusAction.h"
    #include "ui/actions/ShowQRCodeAction.h"
    #include "ui/actions/ShowBarcodeAction.h"
    #include "ui/views/ScrollListView.h"
    #include "ui/views/LogView.h"
    #include "ui/views/ProgressView.h"          // ProgressView::show("msg", pct) — non-blocking

---

## ScrollListView

    ScrollListView view;
    ScrollListView::Row rows[N] = { {"Label", "value"} };
    view.setRows(rows, N);
    view.render(bodyX(), bodyY(), bodyW(), bodyH());
    view.onNav(dir);   // DIR_UP/DIR_DOWN, returns true if consumed

    Store rows[] as a class member so values persist between renders.

---

## LogView

    LogView log;
    log.addLine("Scanning 192.168.1.1...");   // auto-scrolls when full (30 lines max)
    log.draw(bodyX(), bodyY(), bodyW(), bodyH());
    log.draw(bodyX(), bodyY(), bodyW(), bodyH(), statusCallback, userData);  // with status bar
    log.clear();
    log.count();

    Use for scanning/progress screens instead of raw _logLines[] arrays.

---

## ProgressView

    ProgressView::show("Message", pct)   non-blocking progress bar (0-100%), auto word-wrap, max 2 lines

---

## IScreen Power Inhibit

    bool inhibitPowerSave() override { return true; }   // keep display always on (no screen-off, no power-off)
    bool inhibitPowerOff()  override { return true; }   // allow screen-off but block auto power-off

    Override in screen .h files.

    inhibitPowerOff:  use when background process must keep running but screen can turn off
                      (WiFi attacks, GPS wardrive, EAPOL capture, scanning, BLE spam, etc.)
    inhibitPowerSave: use ONLY when screen must stay visible — games during play, MJPEG streaming,
                      real-time visual displays (packet monitor, chat). Implies inhibitPowerOff.

    When display is off (Uni.lcdOff == true):
    - onUpdate() still runs — background logic (GPS, EAPOL capture, Karma, etc.) keeps working
    - render() is blocked by BaseScreen guard — no wasted sprite draws
    - On wake-up, main.cpp forces render() to restore the display immediately

---

## Config System

    Config.load(Uni.Storage)              load from /unigeek/config (call in setup() after storage init)
    Config.save(Uni.Storage)              write to /unigeek/config (call after any set())
    Config.get("key", "default")          returns String value
    Config.set("key", "value")            sets in-memory value (must call save() to persist)
    Config.getThemeColor()                maps APP_CONFIG_PRIMARY_COLOR name → uint16_t TFT color

    All config keys are #defined in ConfigManager.h as APP_CONFIG_* / APP_CONFIG_*_DEFAULT pairs.
    Config.load() must be called after _checkStorageFallback() in setup().
    setup() must apply saved volume after Config.load(): if (Uni.Speaker) Uni.Speaker->setVolume(...)

---

## Storage API

    Uni.Storage->isAvailable()
    Uni.Storage->exists("/path/file.txt")
    Uni.Storage->readFile("/path/file.txt")              returns String
    Uni.Storage->writeFile("/path/file.txt", "content")  returns bool
    Uni.Storage->makeDir("/path")
    Uni.Storage->deleteFile("/path/file.txt")
    Uni.Storage->freeBytes()                             returns uint64_t free bytes
    Uni.Storage->open("/path/file.pcap", FILE_WRITE)     returns fs::File (binary access)
    Uni.Storage->open("/path/file.pcap", FILE_APPEND)    returns fs::File (binary append)
    Uni.Storage->listDir("/path", out, max)              returns uint8_t count

    Use fs::File (not File) when SD.h is not included — fs::File is available from <FS.h> alone.
    StorageSD::writeFile does NOT auto-create parent dirs — call makeDir() first.
    StorageLFS::writeFile calls _makeDir() internally, so parent creation is automatic.
    Uni.Storage may be reassigned at runtime by _checkStorageFallback() in main.cpp when SD fails.

---

## Long-press Pattern for ListScreen Subclasses

    1. Add heldDuration() check in onUpdate() override BEFORE calling ListScreen::onUpdate()
    2. Set a _holdFired flag on trigger to suppress the release event
    3. On next wasPressed(), consume it + readDirection(), clear flag, return

---

## Achievement System

`AchievementManager` (singleton, `src/core/AchievementManager.h`) manages the catalog and progress.
`#define Achievement AchievementManager::getInstance()` — use everywhere.

    Achievement.inc("wifi_first_scan")        // increments counter; auto-unlocks catalog entries
    Achievement.setMax("flappy_score_25", 25) // records a high-water mark
    Achievement.isUnlocked("wifi_first_scan") // bool — used by AchievementScreen
    Achievement.getInt("wifi_first_scan")     // raw counter value
    Achievement.getExp()                      // total accumulated EXP

Toast is drawn from `BaseScreen::update()` automatically — screens never call it directly.

**Rule: when creating a new screen or menu with meaningful user actions, add achievements.**

- First-use action (first scan, first send, first connect) → bronze tier (domain 0-10)
- Repeated-use milestones (5x, 10x, 20x) → silver / gold
- High-skill or rare outcomes (cracked, exploit, full dump) → gold / platinum
- Add entries to `catalog()` in `AchievementManager.h`, then call `Achievement.inc()` or
  `Achievement.setMax()` at the right moment in the screen implementation.

Catalog lives in `static const AchDef* catalog()` as a static constexpr local (avoids linker errors).

### Domains and Tiers

    domain 0  WiFi Network    domain 1  WiFi Attacks   domain 2  Bluetooth
    domain 3  Keyboard        domain 4  NFC            domain 5  IR
    domain 6  Sub-GHz         domain 7  GPS            domain 8  Utility
    domain 9  Games           domain 10 Settings

    tier 0 bronze  +100 EXP    tier 1 silver +300 EXP
    tier 2 gold    +600 EXP    tier 3 platinum +1000 EXP

---

## Body Sprite Rule

    TFT_eSprite sp(&Uni.Lcd);
    sp.createSprite(bodyW(), bodyH());   // ALWAYS bodyW x bodyH — never lcd.width/height
    // ...draw...
    sp.pushSprite(bodyX(), bodyY());     // ALWAYS push at bodyX, bodyY
    sp.deleteSprite();

    bodyW/bodyH already subtract 4px padding — do NOT subtract more inside the sprite.
    Always call deleteSprite() after createSprite() + pushSprite() — TFT_eSprite leaks heap otherwise.
    Only bypass body bounds when user explicitly says to ignore body boundaries.

---

## Partial-Redraw Pattern (Flicker Mitigation)

NEVER create a full-screen or full-body sprite that re-renders every frame —
that is the #1 source of flicker on menu / tutorial / list / dashboard screens.
New menus MUST follow this pattern. Instead of one big sprite:

  1. Draw static chrome (dividers, borders, fixed labels) directly to Uni.Lcd
     ONCE. Guard with a `bool _chromeDrawn = false;` member, reset in onInit().
     Set true after the first draw; subsequent renders skip it.
  2. For each dynamic region (row, zone, button, tile) create a sprite sized to
     that region only: createSprite(regionW, regionH), pushSprite(regionX, regionY).
     Push at absolute screen coordinates — do NOT offset inside the sprite.
  3. Track per-region packed state (e.g. `uint32_t _lastZone[N]` initialized to
     `0xFFFFFFFFu`, or a small hash of the row's displayed values) and skip the
     redraw when state hasn't changed. Reset to 0xFFFFFFFF in onInit() to force
     first paint.
  4. One-shot overlays (done dialog, success badge, completion toast) render
     once via a `bool _doneDrawn = false;` flag — check before drawing, set true
     after pushing.
  5. When a scrolling list can shrink (visible rows < previous frame) clear the
     tail with a single `lcd.fillRect(bodyX(), bodyY() + usedH, bodyW(),
     bodyH() - usedH, TFT_BLACK)` — do NOT wipe the whole body with
     `fillSprite(TFT_BLACK)` on a full-body sprite.

Reference implementations (post-commit d12dc27):
  - TouchGuideScreen  — per-zone sprites + chrome-once + packed state + one-shot dialog
  - AchievementScreen — per-row sprites + tail fillRect for shrinking lists

Exemption: full-body sprites remain acceptable ONLY when the whole body
genuinely repaints every frame (games, MJPEG, packet monitor, audio scopes,
flappy/wordle-style visuals). Residual flicker there is unavoidable — do not
force artificial partial redraws that add complexity for no visible gain.

Flicker-reduction checklist when authoring a new menu:
  ✓ Static chrome drawn once under a `_chromeDrawn` guard
  ✓ Dynamic content broken into the smallest meaningful regions
  ✓ Per-region state cached; redraw skipped when unchanged
  ✓ fillSprite(TFT_BLACK) does not appear on a body-sized sprite
  ✓ Shrinking lists clear their tail with lcd.fillRect, not a full repaint

---

## Sublabel Pattern

    // Pointers must point into class member Strings — NOT temporaries
    _nameSub = Config.get(APP_CONFIG_DEVICE_NAME, APP_CONFIG_DEVICE_NAME_DEFAULT);
    _items[0].sublabel = _nameSub.c_str();

---

## BLE Scan Pattern (NimBLE 1.4.x)

    // Non-blocking scan — NEVER use start(duration, false) which blocks the main loop
    static void scanDoneCB(NimBLEScanResults) {}
    _bleScan = NimBLEDevice::getScan();
    _bleScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
    _bleScan->start(1, scanDoneCB);   // 1s non-blocking scan with callback

    // Re-trigger in onUpdate():
    if (_scanning && _bleScan && !_bleScan->isScanning()) {
      _bleScan->clearResults();
      _bleScan->start(1, scanDoneCB);
    }

    // Use NimBLEAdvertisedDeviceCallbacks (NOT NimBLEScanCallbacks — doesn't exist in 1.4.x)
    // Use setAdvertisedDeviceCallbacks() (NOT setScanCallbacks())