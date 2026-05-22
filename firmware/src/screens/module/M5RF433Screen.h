//
// M5 RF433 (M5 RF433T/R Unit) Screen — single-pin RF, fixed 433.92 MHz.
// Thin wrapper around RfCaptureScreen — the base owns all the shared
// receive/send/jam/browse UI; this file just plugs in the radio backend.
//

#pragma once

#include "screens/module/RfCaptureScreen.h"
#include "utils/rf/M5RF433Util.h"

class M5RF433Screen : public RfCaptureScreen {
public:
  void onInit() override;

protected:
  // ── Radio adapter ──────────────────────────────────────────────────────
  bool        _radioBeginReceive()                override;
  void        _radioEndReceive()                  override { _rf.endReceive(); }
  bool        _radioPollReceive(Signal& out)      override { return _rf.pollReceive(out); }
  bool        _radioSendFromBrowse(const Signal& sig) override;
  void        _radioSendCaptured(const Signal& sig)   override { _rf.sendSignal(sig); }
  bool        _radioStartJam()                    override { _rf.startJam(); return true; }
  void        _radioStopJam()                     override { _rf.stopJam(); }
  void        _radioJamBurst()                    override { _rf.jamBurst(); }
  RxFilter    _radioGetRxFilter()                 override { return _rf.getRxFilter(); }
  void        _radioSetRxFilter(RxFilter f)       override { _rf.setRxFilter(f); }
  void        _radioFreqLabel(char* buf, size_t n) override { snprintf(buf, n, "433.92 MHz"); }
  void        _radioShutdown()                    override { _rf.end(); }
  const char* _titlePrefix()                      override { return "M5 RF433"; }

  void _showMenu()                          override;
  void _onMenuSelected(uint8_t index)       override;

private:
  M5RF433Util _rf;
  int8_t _txPin = -1;
  int8_t _rxPin = -1;

  static constexpr uint8_t kMenuCount = 4;
  ListItem _menuItems[kMenuCount] = {
    {"Receive"},
    {"Send"},
    {"Jammer"},
    {"Mfcodes"},
  };
  String _mfcodesSub;
  void _updateMfcodesSub();
  void _reloadMfcodes();
};
