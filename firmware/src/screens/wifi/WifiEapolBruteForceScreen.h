#pragma once

#include <SD.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "ui/templates/ListScreen.h"
#include "ui/views/BrowseFileView.h"

class WifiEapolBruteForceScreen : public ListScreen {
public:
  const char* title() override { return "EAPOL Brute Force"; }
  bool inhibitPowerOff() override { return _state == STATE_CRACKING; }

  ~WifiEapolBruteForceScreen();

  void onInit() override;
  void onUpdate() override;
  void onRender() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

  // ── WPA2 handshake (extracted from PCAP) ─────────────────────────────────
  struct Handshake {
    bool     valid      = false;
    char     ssid[33]   = {};
    uint8_t  ssid_len   = 0;
    uint8_t  ap[6]      = {};
    uint8_t  sta[6]     = {};
    uint8_t  anonce[32]   = {};
    uint8_t  snonce[32]   = {};
    uint8_t  mic[16]      = {};
    uint8_t  eapol[300]   = {};   // M2 body with MIC zeroed
    uint16_t eapol_len    = 0;
    uint8_t  prf_data[76] = {};   // sorted MACs + sorted nonces for PTK derivation
  };

private:
  // ── Dual-core crack ─────────────────────────────────────────────────────
  static constexpr int QUEUE_DEPTH = 8;
  static constexpr int PASS_MAX    = 64;

  struct PwEntry {
    char    pw[PASS_MAX];
    uint8_t len;
  };

  struct CrackCtx {
    Handshake         hs;
    char              wordlistPath[64] = {};
    QueueHandle_t     queue       = nullptr;
    SemaphoreHandle_t doneSem     = nullptr;
    TaskHandle_t      workerHandle = nullptr;
    volatile bool     stop      = false;
    volatile bool     done      = false;
    volatile bool     found     = false;
    char              foundPass[64] = {};
    char              curPass[64]   = {};
    volatile uint32_t tested     = 0;
    volatile uint32_t bytesDone  = 0;
    volatile uint32_t fileSize   = 0;
    volatile float    speed      = 0.0f;
  };

  static CrackCtx     _ctx;
  static TaskHandle_t _taskHandle;
  static void _crackTask(void* param);    // producer + cracker on core 1
  static void _workerTask(void* param);   // worker cracker on core 0

  // ── State machine ─────────────────────────────────────────────────────────
  enum State { STATE_MENU, STATE_SELECT_PCAP, STATE_SELECT_WORDLIST, STATE_CRACKING, STATE_DONE };
  State _state = STATE_MENU;

  // ── File browser (BrowseFileView + virtual "Built In" tail entry) ────────
  static const char* PCAP_DIR;
  static const char* PASS_DIR;

  BrowseFileView _browser;
  // BrowseFileView's items plus an optional trailing "Built In" virtual entry
  // (wordlist mode at PASS_DIR only). +1 covers the extra slot.
  ListItem _combinedItems[BrowseFileView::kCap + 1] = {};
  uint8_t  _combinedCount = 0;
  bool     _hasBuiltIn    = false;

  String _currentDir;   // current browsing directory

  char _selectedPcap[64]     = {};
  char _selectedWordlist[64] = {};

  // ── Main menu ─────────────────────────────────────────────────────────────
  ListItem _menuItems[3]  = {};
  String   _pcapSub;
  String   _wordlistSub;

  bool     _chromeDrawn = false;

  // ── Helpers ───────────────────────────────────────────────────────────────
  void _showMenu();
  // Reloads the picker for the current state + _currentDir. Returns true if
  // anything is in the list (BrowseFileView entries + optional Built In).
  bool _reloadPicker();
  bool _parsePcap(const char* path);
  void _startCrack();
  void _stopCrack();
  void _saveCrackedPassword();
  void _renderCracking();
  void _renderDone();
};
