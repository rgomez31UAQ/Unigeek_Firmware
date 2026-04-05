//
// RCSwitchUtil — RC switch encode/decode for CC1101
// Derived from RCSwitch by Suat Özgür (LGPL v2.1)
//

#include "RCSwitchUtil.h"

// ── Protocol table ────────────────────────────────────────────────────────
// { pulseLength, syncFactor{H,L}, zero{H,L}, one{H,L}, inverted }

static const DRAM_ATTR RCSwitchUtil::Protocol kProto[] = {
  { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false }, // 1  Princeton
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false }, // 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false }, // 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false }, // 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false }, // 5
  { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true  }, // 6  HT6P20B
  { 150, {  2, 62 }, {  1,  6 }, {  6,  1 }, false }, // 7  HS2303-PT
  { 200, {  3,130 }, {  7, 16 }, {  3, 16 }, false }, // 8  Conrad RS-200 RX
  { 200, {130,  7 }, { 16,  7 }, { 16,  3 }, true  }, // 9  Conrad RS-200 TX
  { 365, { 18,  1 }, {  3,  1 }, {  1,  3 }, true  }, // 10 1ByOne Doorbell
  { 270, { 36,  1 }, {  1,  2 }, {  2,  1 }, true  }, // 11 HT12E
  { 320, { 36,  1 }, {  1,  2 }, {  2,  1 }, true  }, // 12 SM5212
  { 100, {  3,100 }, {  3,  8 }, {  8,  3 }, false }, // 13 Mumbi RC-10
  { 500, {  1, 14 }, {  1,  3 }, {  3,  1 }, false }, // 14 Blyss Doorbell
  { 415, {  1, 30 }, {  1,  3 }, {  4,  1 }, false }, // 15 sc2260R4
  { 250, { 20, 10 }, {  1,  1 }, {  3,  1 }, false }, // 16 Home NetWerks
  {  80, {  3, 25 }, {  3, 13 }, { 11,  5 }, false }, // 17 ORNO OR-GB-417GD
  {  82, {  2, 65 }, {  3,  5 }, {  7,  1 }, false }, // 18 CLARUS BHC993BF-3
  { 560, { 16,  8 }, {  1,  1 }, {  1,  3 }, false }, // 19 NEC
  { 250, {  1,  3 }, {  2,  1 }, {  1,  2 }, false }, // 20 CAME 12bit
  { 330, {  1, 34 }, {  2,  1 }, {  1,  2 }, false }, // 21 FAAC 12bit
  { 700, {  1, 36 }, {  2,  1 }, {  1,  2 }, false }, // 22 NICE 12bit
  { 400, {  0, 10 }, {  2,  1 }, {  1,  2 }, false }, // 23 KeeLoq
};
static constexpr int kNumProto = sizeof(kProto) / sizeof(kProto[0]);

// ── Static storage ────────────────────────────────────────────────────────

volatile unsigned long long RCSwitchUtil::_rxValue      = 0;
volatile unsigned int       RCSwitchUtil::_rxBits        = 0;
volatile unsigned int       RCSwitchUtil::_rxDelay       = 0;
volatile unsigned int       RCSwitchUtil::_rxProto       = 0;
int                         RCSwitchUtil::_rxTolerance   = 60;
int                         RCSwitchUtil::_rxPin         = -1;
unsigned int  RCSwitchUtil::_timings[RCSwitchUtil::kMaxChanges]  = {};
unsigned int  RCSwitchUtil::_rawTimings[RCSwitchUtil::kRawMax]   = {};
unsigned long RCSwitchUtil::_lastMicros    = 0;
unsigned int  RCSwitchUtil::_rawTransitions= 0;

// ── Constructor ───────────────────────────────────────────────────────────

RCSwitchUtil::RCSwitchUtil() {
  setProtocol(1);
}

// ── Protocol / pulse config ────────────────────────────────────────────────

void RCSwitchUtil::setProtocol(int n) {
  if (n < 1 || n > kNumProto) n = 1;
  _proto = kProto[n - 1];
}

void RCSwitchUtil::setProtocol(int n, int pulseLength) {
  setProtocol(n);
  _proto.pulseLength = (uint16_t)pulseLength;
}

void RCSwitchUtil::setPulseLength(int us) {
  _proto.pulseLength = (uint16_t)us;
}

void RCSwitchUtil::setRepeatTransmit(int n) {
  _nRepeat = n;
}

// ── Transmit ──────────────────────────────────────────────────────────────

void RCSwitchUtil::enableTransmit(int pin) {
  _txPin = pin;
  pinMode(pin, OUTPUT);
}

void RCSwitchUtil::disableTransmit() {
  _txPin = -1;
}

void RCSwitchUtil::_transmit(HighLow pulses) {
  uint8_t hi = _proto.invertedSignal ? LOW : HIGH;
  uint8_t lo = _proto.invertedSignal ? HIGH : LOW;

  if (pulses.high) {
    digitalWrite(_txPin, hi);
    uint32_t us = (uint32_t)_proto.pulseLength * pulses.high;
    delay(us / 1000);
    delayMicroseconds(us % 1000);
  }
  if (pulses.low) {
    digitalWrite(_txPin, lo);
    uint32_t us = (uint32_t)_proto.pulseLength * pulses.low;
    delay(us / 1000);
    delayMicroseconds(us % 1000);
  }
}

void RCSwitchUtil::send(unsigned long long code, unsigned int bits) {
  if (_txPin < 0) return;

  bool keeloq = (_proto.syncFactor.high == 0);

  for (int rep = 0; rep < _nRepeat; rep++) {
    if (keeloq) {
      for (int i = 0; i < 11; i++) _transmit({ 1, 1 });
      _transmit({ 1, 10 });
    }

    for (int i = (int)bits - 1; i >= 0; i--) {
      if (code & (1ULL << i))
        _transmit(_proto.one);
      else
        _transmit(_proto.zero);
    }

    if (!keeloq) {
      _transmit(_proto.syncFactor);
    } else {
      _transmit(_proto.one);
      _transmit({ 1, 0 });
      _transmit({ 0, 40 });
    }
  }

  digitalWrite(_txPin, LOW);
}

// ── Receive ───────────────────────────────────────────────────────────────

void RCSwitchUtil::enableReceive(int pin) {
  _rxPin = pin;
  _rxValue = 0;
  _rxBits  = 0;
  attachInterrupt(digitalPinToInterrupt(pin), _isr, CHANGE);
}

void RCSwitchUtil::disableReceive() {
  if (_rxPin >= 0) {
    detachInterrupt(digitalPinToInterrupt(_rxPin));
    _rxPin = -1;
  }
}

void RCSwitchUtil::resetAvailable() {
  _rawTransitions = 0;
  _rxValue = 0;
  memset(_rawTimings, 0, sizeof(_rawTimings));
  _lastMicros = micros() + 10000; // 10ms settle
}

bool RCSwitchUtil::available() {
  return _rxValue != 0;
}

bool RCSwitchUtil::RAWavailable() {
  return _rawTransitions > 10 && (micros() - _lastMicros) > 100000;
}

// ── Protocol decoder (called from ISR) ────────────────────────────────────

bool RCSwitchUtil::_receiveProtocol(int p, unsigned int changeCount) {
  const Protocol& pro = kProto[p - 1];

  unsigned long long code = 0;
  const unsigned int syncLen = (pro.syncFactor.low > pro.syncFactor.high)
                               ? pro.syncFactor.low : pro.syncFactor.high;
  unsigned int te  = _timings[0] / syncLen;
  unsigned int tol = te * _rxTolerance / 100;

  // Protocol 23 (KeeLoq): fixed timing
  if (p == 23) { te = 400; tol = 400 * _rxTolerance / 100; }

  unsigned int firstData = pro.invertedSignal ? 2u : 1u;
  if (p == 23) firstData = 25;

  unsigned int numBits = 0;
  for (unsigned int i = firstData; i < changeCount - 1 && numBits < 64; i += 2, numBits++) {
    code <<= 1ULL;
    if (abs((int)_timings[i]     - (int)(te * pro.zero.high)) < (int)tol &&
        abs((int)_timings[i + 1] - (int)(te * pro.zero.low))  < (int)tol) {
      // zero — no change
    } else if (abs((int)_timings[i]     - (int)(te * pro.one.high)) < (int)tol &&
               abs((int)_timings[i + 1] - (int)(te * pro.one.low))  < (int)tol) {
      code |= 1ULL;
    } else {
      return false;
    }
  }

  if (changeCount > 7) {
    _rxValue = code;
    _rxBits  = numBits;
    _rxDelay = te;
    _rxProto = (unsigned int)p;
    return true;
  }
  return false;
}

// ── ISR ───────────────────────────────────────────────────────────────────

void RCSwitchUtil::_isr() {
  static unsigned int changeCount    = 0;
  static unsigned int changeRAWCount = 0;
  static unsigned long lastTime      = 0;
  static unsigned int repeatCount    = 0;

  static unsigned long rawPreBuff[kRawPreSize] = {};
  static unsigned int  rawPreCount = 0;

  const unsigned long now      = micros();
  const unsigned int  duration = (unsigned int)(now - lastTime);

  if (duration > kSepLimit) {
    if (repeatCount == 0 ||
        (unsigned int)abs((int)duration - (int)_timings[0]) < 200) {
      repeatCount++;
      if (repeatCount == 2) {
        for (int i = 1; i <= kNumProto; i++) {
          if (_receiveProtocol(i, changeCount)) break;
        }
        repeatCount = 0;
      }
    }
    changeCount = 0;
  }

  if (changeCount >= kMaxChanges) {
    changeCount  = 0;
    repeatCount  = 0;
  }
  _timings[changeCount++] = duration;

  // RAW capture — reset on very long gaps (>400ms)
  if (duration > 400000) {
    changeRAWCount = 0;
    memset(_rawTimings, 0, sizeof(_rawTimings));
    memset(rawPreBuff,  0, sizeof(rawPreBuff));
    rawPreCount = 0;
    lastTime = now;
    return;
  }

  // Sliding-window noise filter: require sum of last kRawPreSize durations >= threshold
  rawPreCount = rawPreCount + duration - rawPreBuff[0];
  for (unsigned int i = 0; i < kRawPreSize - 1; i++) rawPreBuff[i] = rawPreBuff[i + 1];
  rawPreBuff[kRawPreSize - 1] = duration;

  if (rawPreCount >= kRawPreSize * kNoiseThresh) {
    if (changeRAWCount >= kRawMax) changeRAWCount = 0;
    if (duration > kNoiseThresh) {
      _rawTimings[changeRAWCount++] = duration;
      if (changeRAWCount == 15) {
        _rawTransitions = 15;
        _lastMicros     = now;
      }
    }
  } else {
    changeRAWCount = 0;
  }

  lastTime = now;
}