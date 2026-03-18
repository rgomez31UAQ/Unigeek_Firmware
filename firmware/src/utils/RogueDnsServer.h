#pragma once

#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>

class RogueDnsServer {
public:
  static constexpr int MAX_RECORDS = 20;
  static constexpr const char* CONFIG_PATH = "/unigeek/wifi/portals/dns_config";

  struct DnsRecord {
    char domain[64];
    char path[128];
  };

  using VisitCallback = void(*)(const char* clientIP, const char* domain);
  using PostCallback  = void(*)(const char* clientIP, const char* domain, const char* data);

  bool begin(IPAddress apIP);
  void end();
  void update();  // call each frame to process DHCP + DNS packets

  void setVisitCallback(VisitCallback cb) { _visitCb = cb; }
  void setPostCallback(PostCallback cb)  { _postCb = cb; }
  void setCaptivePortalPath(const char* path) { strncpy(_captivePath, path, sizeof(_captivePath) - 1); }
  void setFileManagerEnabled(bool en) { _fileManagerEnabled = en; }

  const DnsRecord* records() const { return _records; }
  uint8_t recordCount() const { return _recordCount; }

private:
  IPAddress _apIP;
  bool _running = false;
  VisitCallback _visitCb = nullptr;
  PostCallback  _postCb  = nullptr;
  char _captivePath[128] = {};
  bool _fileManagerEnabled = false;

  // Config
  DnsRecord _records[MAX_RECORDS];
  uint8_t   _recordCount = 0;
  bool _loadConfig();

  // DHCP server (port 67)
  WiFiUDP _dhcpUdp;
  void _processDhcp();
  void _handleDhcpPacket(uint8_t* buf, int len);
  uint8_t _getDhcpMsgType(uint8_t* buf, int len);
  void _buildDhcpResponse(uint8_t* buf, int& len, uint8_t msgType, uint8_t ipSuffix);
  uint8_t _allocateIp(uint8_t* mac);

  static constexpr int DHCP_POOL_SIZE = 20;
  uint8_t _poolStart = 101;
  bool    _poolUsed[DHCP_POOL_SIZE] = {};
  struct DhcpClient { uint8_t mac[6]; uint8_t suffix; };
  DhcpClient _clients[DHCP_POOL_SIZE] = {};

  // DNS server (port 53)
  WiFiUDP _dnsUdp;
  void _processDns();
  bool _matchDomain(const char* query, const char* config);
  const char* _findPath(const char* domain);

  // Web server (port 80)
  AsyncWebServer* _webServer = nullptr;
  void _startWeb();
  void _stopWeb();
  void _serveFromPath(const char* portalPath, AsyncWebServerRequest* req);
};
