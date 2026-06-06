//
// Created by L Shaf on 2026-02-18.
//

#pragma once
#include <Arduino.h>
#include "ConfigManager.h"

class INavigation
{
public:
  enum Direction
  {
    DIR_NONE,
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_PRESS,
    DIR_BACK
  };

  virtual ~INavigation() = default;
  virtual void update() = 0;
  virtual void begin() = 0;
  virtual void setTouchSwapXY(bool) {}

  // True when this navigation provides distinct UP/DOWN/LEFT/RIGHT input
  // (cardputer, t-embed, CYD touch, etc., or stick in EncoderC-hat mode).
  // False for the default 2-axis stick nav (UP/DOWN/PRESS/BACK only).
  // Screens use this to choose between the canonical 4-way pattern and
  // the LEFT||UP / RIGHT||DOWN fallback aliasing.
  virtual bool is4Way() const {
#ifdef DEVICE_HAS_4WAY_NAV
    return true;
#else
# ifdef DEVICE_HAS_NAV_MODE_SWITCH
    return Config.get(APP_CONFIG_NAV_MODE, APP_CONFIG_NAV_MODE_DEFAULT) == "encoder";
# else
    return false;
# endif
#endif
  }

  // Post-render hook: draws the live edge indicator for the active touch zone.
  // Skipped automatically when suppressKeys is set (e.g. touch-nav overall grid).
  // Board NavigationImpl overrides _doDrawOverlay(); never override drawOverlay().
  void drawOverlay() {
    if (!_suppressKeys) _doDrawOverlay();
  }

  bool isPressed()          const { return _pressed || _injHeld; }
  Direction currentDirection() const { return _injHeld ? _injDir : _currDirection; }

  bool wasPressed() {
    if (_wasPressed) {
      _wasPressed = false;
      return true;
    }
    return false;
  }

  Direction readDirection() {
    Direction dir = _releasedDirection;
    _releasedDirection = DIR_NONE;
    return dir;
  }

  uint32_t pressDuration() const { return _pressDuration; }
  uint32_t heldDuration()  const {
    if (_injHeld) return millis() - _injStart;
    return _pressed ? (millis() - _pressStart) : 0;
  }

  void setSuppressKeys(bool s) { _suppressKeys = s; }
  bool suppressKeys() const    { return _suppressKeys; }

  // Inject a synthetic tap of `dir` (e.g. from the web remote). Because the
  // press/release state lives here in the base, this works on whatever board
  // nav is active with no decorator: the next wasPressed()/readDirection()
  // report a completed press of `dir`. Same-task as the screen poll, so no
  // race. Taps only (no hold) — enough for 6-way menu navigation.
  void inject(Direction dir) {
    if (dir == DIR_NONE) return;
    _wasPressed = true;
    _releasedDirection = dir;
  }

  // One-shot remote long-press (web "m"). A single call holds `dir` for
  // kInjectHoldMs so isPressed()/heldDuration() report a long hold — long-press
  // handlers (FileManager / ListScreen "hold to open menu") fire — then
  // serviceInject() auto-releases it, emitting the completed press. Tracked
  // separately from hardware so the board's per-tick updateState never cancels
  // it; no host key-up needed.
  static constexpr uint32_t kInjectHoldMs = 1000;
  void injectHold(Direction dir) {
    if (dir == DIR_NONE) return;
    _injHeld = true; _injDir = dir; _injStart = millis();
  }

  // Auto-release an expired injected hold. Called once per loop by
  // Device::update() (cheap: one bool + time check).
  void serviceInject() {
    if (_injHeld && (millis() - _injStart) >= kInjectHoldMs) {
      _injHeld = false;
      _pressDuration = millis() - _injStart;
      _wasPressed = true;
      _releasedDirection = _injDir;
    }
  }

  int16_t lastTouchX() const { return _lastTouchX; }
  int16_t lastTouchY() const { return _lastTouchY; }

  virtual void setRightHand(bool v) { _rightHand = v; }

  // Used by main.cpp when a press wakes the display from power save: clear any
  // pending release event and, if a press is currently in progress, drop its
  // future release so it never propagates as an action. The wake-up press only
  // turns the screen on — it must not double as an action press.
  void suppressCurrentPress() {
    _wasPressed = false;
    _releasedDirection = DIR_NONE;
    _suppressRelease = _pressed;
  }

protected:
  virtual void _doDrawOverlay() {}

  int16_t _lastTouchX = -1;
  int16_t _lastTouchY = -1;

  Direction orientDir(Direction d) const {
    if (_rightHand) {
      if (d == DIR_UP)   return DIR_DOWN;
      if (d == DIR_DOWN) return DIR_UP;
    }
    return d;
  }

  void updateState(Direction currentlyHeld) {
    uint32_t now = millis();

    if (currentlyHeld != DIR_NONE) {
      if (!_pressed) {
        _pressed = true;
        _pressStart = now;
      }
      _currDirection = currentlyHeld;

    } else {
      if (_pressed) {
        _pressed = false;
        _pressDuration = now - _pressStart;
        if (_suppressRelease) {
          _suppressRelease = false;
          _currDirection = DIR_NONE;
        } else {
          _wasPressed = true;
          _releasedDirection = _currDirection;
          _currDirection = DIR_NONE;
        }
      }
    }
  }

private:
  Direction _currDirection    = DIR_NONE;
  Direction _releasedDirection = DIR_NONE;

  bool     _pressed         = false;
  bool     _wasPressed      = false;

  // Injected held-press state (web remote long-press), separate from hardware.
  bool      _injHeld  = false;
  Direction _injDir   = DIR_NONE;
  uint32_t  _injStart = 0;

  bool     _suppressKeys    = false;
  bool     _suppressRelease = false;
  bool     _rightHand       = false;

  uint32_t _pressStart   = 0;
  uint32_t _pressDuration = 0;
};