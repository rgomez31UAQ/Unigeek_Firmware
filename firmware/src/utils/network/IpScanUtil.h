#pragma once
#include <cstdint>

// ICMP-ping-based host discovery on the current WiFi /24 subnet.
// Uses lwip raw sockets — works on ESP32 WiFi (ARP is unreliable there).
class IpScanUtil {
public:
  struct Host {
    char ip[16];
    char hostname[50];
  };

  // Scans startOctet..endOctet on the local subnet via ICMP ping (100 ms timeout/host).
  // resolveHostnames: attempt reverse DNS on found hosts.
  // progressCb: optional, called with 0–100 during scan.
  // Returns number of live hosts found, written to out[].
  static uint8_t scan(uint8_t startOctet, uint8_t endOctet,
                      Host* out, uint8_t maxHosts,
                      bool resolveHostnames = false,
                      void (*progressCb)(uint8_t) = nullptr);
};