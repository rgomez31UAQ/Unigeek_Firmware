#!/usr/bin/env python3
"""
Fake network printer for testing UniGeek's Printer Prank screen.

Pretends to be a network printer on the LAN: responds to SSDP M-SEARCH
for `urn:schemas-upnp-org:device:Printer:1` and accepts JetDirect raw
print jobs on TCP port 9100. Prints whatever the device sent to stdout.

Usage:
    python3 fake-printer.py
    python3 fake-printer.py --name "Office HP" --port 9100

Stdlib only — no external dependencies.

Notes:
- macOS / Linux: works as user. Windows: run as Administrator (UDP 1900 bind).
- Same firewall caveats as fake-dial.py.
"""

import argparse
import socket
import socketserver
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer


def local_ip() -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


# ── HTTP server (UPnP description) ────────────────────────────────────────────


def make_handler(ip: str, http_port: int, friendly_name: str):
    device_xml = (
        '<?xml version="1.0"?>\n'
        '<root xmlns="urn:schemas-upnp-org:device-1-0">\n'
        '  <specVersion><major>1</major><minor>0</minor></specVersion>\n'
        '  <device>\n'
        '    <deviceType>urn:schemas-upnp-org:device:Printer:1</deviceType>\n'
        f'    <friendlyName>{friendly_name}</friendlyName>\n'
        '    <manufacturer>UniGeek Test</manufacturer>\n'
        '    <modelName>FakePrint 9000</modelName>\n'
        '    <UDN>uuid:fake-printer-test-1234</UDN>\n'
        '  </device>\n'
        '</root>\n'
    ).encode("utf-8")

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path in ("/dd.xml", "/dd.xml/"):
                self.send_response(200)
                self.send_header("Content-Type", "application/xml")
                self.send_header("Content-Length", str(len(device_xml)))
                self.end_headers()
                self.wfile.write(device_xml)
                print(f"[HTTP] {self.client_address[0]} GET {self.path}")
                return
            self.send_response(404)
            self.end_headers()

        def log_message(self, fmt, *args):
            pass

    return Handler


# ── SSDP responder ───────────────────────────────────────────────────────────


def ssdp_responder(ip: str, http_port: int, stop_event: threading.Event):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except (AttributeError, OSError):
        pass
    sock.bind(("", 1900))

    mreq = struct.pack(
        "4s4s",
        socket.inet_aton("239.255.255.250"),
        socket.inet_aton(ip),
    )
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.settimeout(0.5)

    print(f"[SSDP] Listening on 239.255.255.250:1900 (bound iface {ip})")

    while not stop_event.is_set():
        try:
            data, addr = sock.recvfrom(8192)
        except socket.timeout:
            continue
        except OSError:
            break

        text = data.decode("utf-8", errors="replace")
        if "M-SEARCH" not in text:
            continue
        if not (
            "device:printer" in text.lower()
            or "ssdp:all" in text.lower()
            or "upnp:rootdevice" in text.lower()
        ):
            continue

        reply = (
            "HTTP/1.1 200 OK\r\n"
            "CACHE-CONTROL: max-age=1800\r\n"
            "EXT: \r\n"
            f"LOCATION: http://{ip}:{http_port}/dd.xml\r\n"
            "SERVER: Linux/3.10 UPnP/1.0 FakePrinter/1.0\r\n"
            "ST: urn:schemas-upnp-org:device:Printer:1\r\n"
            "USN: uuid:fake-printer-test-1234::urn:schemas-upnp-org:device:Printer:1\r\n"
            "BOOTID.UPNP.ORG: 1\r\n"
            "CONFIGID.UPNP.ORG: 1\r\n"
            "\r\n"
        )
        try:
            sock.sendto(reply.encode("utf-8"), addr)
            print(f"[SSDP] Replied to M-SEARCH from {addr[0]}:{addr[1]}")
        except OSError as e:
            print(f"[SSDP] send error: {e}")

    sock.close()


# ── JetDirect raw print server (port 9100) ───────────────────────────────────


class RawPrintHandler(socketserver.BaseRequestHandler):
    def handle(self):
        chunks = []
        self.request.settimeout(2.0)
        try:
            while True:
                data = self.request.recv(4096)
                if not data:
                    break
                chunks.append(data)
        except socket.timeout:
            pass
        body = b"".join(chunks)

        print()
        print("*" * 60)
        print(f" PRINT JOB RECEIVED  from {self.client_address[0]}")
        print(f" Bytes: {len(body)}")
        print("*" * 60)
        # Decode best-effort and strip control characters that don't print
        text = body.decode("latin-1", errors="replace")
        # Print the raw text; PJL escape sequences will appear inline
        print(text)
        print("*" * 60)
        print()


class ThreadedTCP(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True


def main():
    ap = argparse.ArgumentParser(description="Fake JetDirect printer for UniGeek Printer Prank testing.")
    ap.add_argument("--http-port", type=int, default=56790, help="HTTP port for UPnP description")
    ap.add_argument("--port",      type=int, default=9100,  help="TCP port for raw print jobs (JetDirect)")
    ap.add_argument("--name",      type=str, default="Fake Office Printer", help="Friendly name in device description")
    ap.add_argument("--ip",        type=str, default=None,  help="Override auto-detected local IP")
    args = ap.parse_args()

    ip = args.ip or local_ip()

    print(f"Local IP:        {ip}")
    print(f"HTTP port:       {args.http_port}")
    print(f"Print port:      {args.port}")
    print(f"Friendly name:   {args.name}")
    print(f"Description URL: http://{ip}:{args.http_port}/dd.xml")
    print()
    print("Open Printer Prank on UniGeek and tap 'Discover & Print'.")
    print("Press Ctrl+C to stop.")
    print()

    stop_event = threading.Event()

    Handler = make_handler(ip, args.http_port, args.name)
    http = HTTPServer(("0.0.0.0", args.http_port), Handler)
    http_thread = threading.Thread(target=http.serve_forever, daemon=True)
    http_thread.start()

    ssdp_thread = threading.Thread(
        target=ssdp_responder, args=(ip, args.http_port, stop_event), daemon=True
    )
    ssdp_thread.start()

    raw = ThreadedTCP(("0.0.0.0", args.port), RawPrintHandler)
    raw_thread = threading.Thread(target=raw.serve_forever, daemon=True)
    raw_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
        stop_event.set()
        http.shutdown()
        raw.shutdown()


if __name__ == "__main__":
    main()
