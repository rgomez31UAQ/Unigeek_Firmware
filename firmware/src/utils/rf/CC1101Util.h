//
// CC1101 Sub-GHz Utility — send/receive raw RF signals
// Reference: Bruce firmware (https://github.com/pr3y/Bruce)
//

#pragma once
#include <Arduino.h>
#include <functional>
#include <SPI.h>

class CC1101Util {
public:
  static constexpr float DEFAULT_FREQ = 433.92;
  static constexpr uint16_t MAX_RAW_LEN = 2048;

  struct Signal {
    float frequency = 0;     // MHz
    String preset = "0";     // modulation preset name or RcSwitch protocol number
    String protocol = "RAW"; // "RAW", "RcSwitch"
    String rawData;           // signed pulse durations (+ = HIGH, - = LOW) in µs

    // RcSwitch protocol fields
    uint64_t key = 0;         // decoded key value
    int te = 0;               // timing element / pulse length in µs
    int bit = 0;              // number of data bits
  };

  // Initialize CC1101 with CS and GDO0 pins
  bool begin(SPIClass* spi, int8_t csPin, int8_t gdo0Pin,
             int8_t spiSck = -1, int8_t spiMiso = -1, int8_t spiMosi = -1);
  void end();

  // Set frequency in MHz (280–928, valid sub-bands only)
  bool setFrequency(float mhz);
  float getFrequency() const { return _freq; }

  // Check if CC1101 is connected
  bool isConnected();

  // Receive: RSSI-scans for best frequency, then decodes with RCSwitch
  //   - prefers RcSwitch decode; falls back to RAW pulse data
  // cancelCb: called each loop iteration; return true to abort
  bool receive(Signal& out, uint32_t timeoutMs = 20000,
               std::function<bool()> cancelCb = nullptr);

  // Scan status (readable while receive() is running via cancelCb)
  bool  isScanning()   const { return _scanning; }
  float getScanFreq()  const { return _scanFreq; }
  int   getScanRssi()  const { return _scanRssi; }

  // Start TX mode (for jammer — caller controls GDO0 directly)
  void startTx();

  // Send a signal (handles RAW and RcSwitch/Princeton protocols)
  void sendSignal(const Signal& sig);

  // File I/O — Bruce SubGhz .sub format
  static bool loadFile(const String& content, Signal& out);
  static String saveToString(const Signal& sig);

  // Display helper
  static String signalLabel(const Signal& sig);

private:
  int8_t _csPin = -1;
  int8_t _gdo0Pin = -1;
  float  _freq = DEFAULT_FREQ;
  bool   _initialized = false;

  // Scan status (updated during receive())
  bool  _scanning = false;
  float _scanFreq = 0;
  int   _scanRssi = 0;

  float _scanForBestFreq(std::function<bool()> cancelCb);
  void  _initTx();
  void  _initRx();
  void  _sendRcSwitch(const Signal& sig);
};