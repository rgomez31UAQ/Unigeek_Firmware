//
// Sub-GHz (CC1101) Screen — builds on RfCaptureScreen and adds the SubGHz-only
// pieces: 5-item menu (Frequency, Detect Freq, Receive, Send, Jammer),
// frequency selection popup, and the STATE_SCANNING frequency-sweep view.
//

#pragma once

#include "screens/module/RfCaptureScreen.h"
#include "utils/rf/CC1101Util.h"

class SubGHzScreen : public RfCaptureScreen {
public:
  void onInit() override;

protected:
  // Extra states: frequency-sweep view, RSSI waterfall.
  static constexpr int STATE_SCANNING  = STATE_USER_BASE + 0;
  static constexpr int STATE_WATERFALL = STATE_USER_BASE + 1;

  // ── Radio adapter ──────────────────────────────────────────────────────
  bool        _radioBeginReceive()                override { return _rf.beginReceive(); }
  void        _radioEndReceive()                  override { _rf.endReceive(); }
  bool        _radioPollReceive(Signal& out)      override { return _rf.pollReceive(out); }
  bool        _radioSendFromBrowse(const Signal& sig) override;
  void        _radioSendCaptured(const Signal& sig)   override;
  bool        _radioStartJam()                    override;
  void        _radioStopJam()                     override;
  void        _radioJamBurst()                    override;
  RxFilter    _radioGetRxFilter()                 override { return _rf.getRxFilter(); }
  void        _radioSetRxFilter(RxFilter f)       override { _rf.setRxFilter(f); }
  void        _radioFreqLabel(char* buf, size_t n) override {
    snprintf(buf, n, "%.2f MHz", _rf.getFrequency());
  }
  void        _radioShutdown()                    override { _rf.end(); }
  const char* _titlePrefix()                      override { return "Sub-GHz"; }

  void _showMenu()                          override;
  void _onMenuSelected(uint8_t index)       override;

  // ── Extension hooks for STATE_SCANNING ─────────────────────────────────
  bool _onUpdateExtra()             override;
  bool _onRenderExtra()             override;
  bool _onBackExtra()               override;
  bool _inhibitExtra() const        override {
    return _state == STATE_SCANNING || _state == STATE_WATERFALL;
  }

private:
  CC1101Util _rf;
  int8_t _csPin   = -1;
  int8_t _gdo0Pin = -1;
  bool   _rfDetectFired = false;  // achievement guard, resets each scan session

  // Menu (7 items: Frequency | Detect Freq | Waterfall | Receive | Send |
  //                Jammer | Mfcodes)
  static constexpr uint8_t kMenuCount = 7;
  ListItem _menuItems[kMenuCount] = {
    {"Frequency"},
    {"Detect Freq"},
    {"Waterfall"},
    {"Receive"},
    {"Send"},
    {"Jammer"},
    {"Mfcodes"},
  };
  String _freqSub;
  String _mfcodesSub;
  void _updateSublabels();
  void _selectFrequency();
  void _startScan();
  void _reloadMfcodes();

  // ── Waterfall (RSSI spectrogram) ───────────────────────────────────────
  // A scrolling RSSI heat-map across a [start,end] MHz window, swept per pixel
  // over SPI. The pre-run popup picks the band; the run scrolls one freshly
  // swept line per frame.
  float    _wfStart   = 433.42f;  // 1 MHz window centred on 433.92 (common SubGHz)
  float    _wfEnd     = 434.42f;
  int      _wfLine    = 0;        // current waterfall row (body-relative y)
  int      _wfMaxRssi = -120;
  float    _wfMaxFreq = 0;
  uint32_t _wfLastMax = 0;
  void _startWaterfall();         // band-select popup loop, then run
  void _runWaterfall();           // enter STATE_WATERFALL
  void _pickWaterfallFreq(float& boundary);
  bool _onUpdateWaterfall();
  bool _onRenderWaterfall();
  static uint16_t _waterfallColor(int rssi);
};
