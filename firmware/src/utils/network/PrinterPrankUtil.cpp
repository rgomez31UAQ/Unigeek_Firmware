#include "utils/network/PrinterPrankUtil.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <string.h>

static const char* MSEARCH_REQ =
  "M-SEARCH * HTTP/1.1\r\n"
  "HOST: 239.255.255.250:1900\r\n"
  "MAN: \"ssdp:discover\"\r\n"
  "MX: 2\r\n"
  "ST: urn:schemas-upnp-org:device:Printer:1\r\n"
  "\r\n";

static const char* ci_strstr(const char* h, const char* n)
{
  size_t nl = strlen(n);
  for (; *h; h++) {
    if (strncasecmp(h, n, nl) == 0) return h;
  }
  return nullptr;
}

static String extractTag(const String& body, const char* tag)
{
  String openTag  = String("<")  + tag + ">";
  String closeTag = String("</") + tag + ">";
  int s = body.indexOf(openTag);
  if (s < 0) return "";
  s += openTag.length();
  int e = body.indexOf(closeTag, s);
  if (e < 0) return "";
  String v = body.substring(s, e);
  v.trim();
  return v;
}

uint8_t PrinterPrankUtil::discover(Device* out, uint8_t maxDevices,
                                    void (*progressCb)(uint8_t))
{
  if (!out || maxDevices == 0) return 0;
  if (WiFi.status() != WL_CONNECTED) return 0;

  WiFiUDP udp;
  if (!udp.begin(0)) return 0;

  IPAddress mcast(239, 255, 255, 250);

  auto sendMSearch = [&]() {
    if (!udp.beginPacket(mcast, 1900)) return;
    udp.write((const uint8_t*)MSEARCH_REQ, strlen(MSEARCH_REQ));
    udp.endPacket();
  };

  // Wi-Fi multicast is lossy — APs send at 1 Mbps with no retries — so
  // re-emit M-SEARCH up to 3 times during the discovery window. Mirrors
  // what real UPnP clients do.
  sendMSearch();
  uint8_t        searchesSent  = 1;
  const uint8_t  maxSearches   = 3;
  const uint32_t resendEveryMs = 900;

  uint8_t  count    = 0;
  uint32_t startMs  = millis();
  const uint32_t totalMs = 4500;   // slightly longer to absorb retransmits
  uint32_t deadline = startMs + totalMs;
  uint32_t nextResend = startMs + resendEveryMs;

  while (millis() < deadline && count < maxDevices) {
    if (searchesSent < maxSearches && (int32_t)(millis() - nextResend) >= 0) {
      sendMSearch();
      searchesSent++;
      nextResend = millis() + resendEveryMs;
    }
    if (progressCb) {
      uint32_t elapsed = millis() - startMs;
      uint8_t pct = (elapsed >= totalMs) ? 100 : (uint8_t)(elapsed * 100 / totalMs);
      progressCb(pct);
    }

    int sz = udp.parsePacket();
    if (sz <= 0) { delay(50); continue; }

    char buf[768];
    int len = udp.read(buf, sizeof(buf) - 1);
    if (len <= 0) continue;
    buf[len] = '\0';

    IPAddress src = udp.remoteIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);

    bool dup = false;
    for (uint8_t j = 0; j < count; j++) {
      if (strcmp(out[j].ip, ipStr) == 0) { dup = true; break; }
    }
    if (dup) continue;

    const char* loc = ci_strstr(buf, "LOCATION:");
    if (!loc) continue;
    loc += 9;
    while (*loc == ' ' || *loc == '\t') loc++;

    char locUrl[200];
    int li = 0;
    while (li < (int)sizeof(locUrl) - 1 && *loc && *loc != '\r' && *loc != '\n') {
      locUrl[li++] = *loc++;
    }
    locUrl[li] = '\0';
    if (li == 0) continue;

    HTTPClient http;
    http.setTimeout(2000);
    if (!http.begin(locUrl)) continue;
    int code = http.GET();
    String body;
    if (code == 200) body = http.getString();
    http.end();

    String name;
    if (!body.isEmpty()) {
      name = extractTag(body, "friendlyName");
      if (name.isEmpty()) name = extractTag(body, "modelName");
    }
    if (name.isEmpty()) name = "Network Printer";

    strncpy(out[count].ip,   ipStr,        sizeof(out[count].ip)   - 1);
    out[count].ip[sizeof(out[count].ip) - 1] = '\0';
    strncpy(out[count].name, name.c_str(), sizeof(out[count].name) - 1);
    out[count].name[sizeof(out[count].name) - 1] = '\0';
    out[count].port = RAW_PORT;
    count++;
  }

  udp.stop();
  if (progressCb) progressCb(100);
  return count;
}

bool PrinterPrankUtil::printText(const Device& dev, const char* text)
{
  if (!text) return false;

  WiFiClient client;
  client.setTimeout(3000);
  if (!client.connect(dev.ip, dev.port)) return false;

  // PJL header sets language to plain TEXT — most modern printers honour
  // this and skip the language sniffing that occasionally drops raw ASCII.
  String body;
  body.reserve(strlen(text) + 96);
  body  = "\x1B%-12345X@PJL JOB\r\n";
  body += "@PJL ENTER LANGUAGE = TEXT\r\n";
  body += text;
  body += "\r\n\f";                         // form feed → eject page
  body += "\x1B%-12345X@PJL EOJ\r\n";

  size_t total  = body.length();
  const uint8_t* p = (const uint8_t*)body.c_str();
  size_t written = 0;
  while (written < total) {
    size_t n = client.write(p + written, total - written);
    if (n == 0) { client.stop(); return false; }
    written += n;
  }
  client.flush();
  client.stop();
  return true;
}
