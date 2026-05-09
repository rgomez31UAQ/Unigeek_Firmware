#pragma once
#include <Arduino.h>
#include <cstdint>

// Discovers UPnP-advertised printers on the local network and submits
// raw print jobs over JetDirect (TCP port 9100). Port 9100 is unauthenticated
// on virtually every consumer/office network printer — that's the whole
// "prank" attack surface.
class PrinterPrankUtil
{
public:
  struct Device {
    char     name[32];
    char     ip[16];
    uint16_t port;        // 9100 by default
  };

  static constexpr uint8_t MAX_DEVICES = 16;
  static constexpr uint16_t RAW_PORT   = 9100;

  // SSDP M-SEARCH (~3.5 s) for `urn:schemas-upnp-org:device:Printer:1`.
  // Resolves each unique responder's description URL for friendly name.
  static uint8_t discover(Device* out, uint8_t maxDevices,
                          void (*progressCb)(uint8_t) = nullptr);

  // Open TCP to dev.ip:dev.port, send `text` + form-feed, close.
  // Plain ASCII; modern printers auto-detect language.
  static bool printText(const Device& dev, const char* text);
};
