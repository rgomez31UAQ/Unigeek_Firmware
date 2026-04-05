//
// RCSwitchUtil — RC switch encode/decode for CC1101
// Derived from RCSwitch by Suat Özgür (LGPL v2.1)
// Stripped to send + interrupt-based receive only.
//

#pragma once
#include <Arduino.h>

class RCSwitchUtil {
public:
  struct HighLow { uint8_t high; uint8_t low; };

  struct Protocol {
    uint16_t pulseLength;   // base pulse length in µs
    HighLow  syncFactor;
    HighLow  zero;
    HighLow  one;
    bool     invertedSignal;
  };

  RCSwitchUtil();

  // ── Transmit ──────────────────────────────────────────────────────────────
  void enableTransmit(int pin);
  void disableTransmit();
  void setProtocol(int n);
  void setProtocol(int n, int pulseLength);
  void setPulseLength(int us);
  void setRepeatTransmit(int n);
  void send(unsigned long long code, unsigned int bits);

  // ── Receive ───────────────────────────────────────────────────────────────
  void enableReceive(int pin);
  void disableReceive();
  void resetAvailable();

  bool available();
  bool RAWavailable();

  unsigned long long getReceivedValue()    { return _rxValue; }
  unsigned int       getReceivedBitlength(){ return _rxBits;  }
  unsigned int       getReceivedDelay()    { return _rxDelay; }
  unsigned int       getReceivedProtocol() { return _rxProto; }
  unsigned int*      getRAWReceivedRawdata(){ return _rawTimings; }

private:
  int      _txPin    = -1;
  int      _nRepeat  = 10;
  Protocol _proto;

  void _transmit(HighLow pulses);

  static void _isr();
  static bool           _receiveProtocol(int p, unsigned int changeCount);

  // shared ISR state
  static volatile unsigned long long _rxValue;
  static volatile unsigned int       _rxBits;
  static volatile unsigned int       _rxDelay;
  static volatile unsigned int       _rxProto;
  static int                         _rxTolerance;
  static int                         _rxPin;

  static constexpr unsigned int kSepLimit    = 4300;
  static constexpr unsigned int kMaxChanges  = 157;
  static constexpr unsigned int kRawMax      = 512;
  static constexpr unsigned int kNoiseThresh = 50;
  static constexpr unsigned int kRawPreSize  = 5;

  static unsigned int  _timings[kMaxChanges];
  static unsigned int  _rawTimings[kRawMax];
  static unsigned long _lastMicros;
  static unsigned int  _rawTransitions;
};