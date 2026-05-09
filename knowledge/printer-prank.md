# Printer Prank

Discovers UPnP-advertised network printers on the LAN and submits short joke print jobs over JetDirect (TCP port 9100). Port 9100 is unauthenticated on virtually every consumer/office network printer — drop bytes on it and they get printed.

## Usage

1. Connect to a WiFi network: **WiFi > Network**
2. Open **Printer Prank**
3. **Message** — tap to cycle through presets (Affirmation, Hydrate, Compliment, Update Required, Hello Void)
4. **Custom Message** — enter your own short text
5. **Discover & Print** — scans the LAN for ~3.5 s
6. Tap a discovered printer to send the job; tap **Print to All** (when 2+ printers) to fire at every printer

## How it works

1. **Discovery** — sends an SSDP `M-SEARCH` to multicast `239.255.255.250:1900` with `ST: urn:schemas-upnp-org:device:Printer:1`
2. Listens for unicast SSDP responses (3.5 s window)
3. For each unique responder, fetches the `LOCATION:` URL and parses `<friendlyName>` / `<modelName>` from the device description XML
4. **Print** — opens TCP to `<ip>:9100`, writes a small PJL/text envelope:
   ```
   <ESC>%-12345X@PJL JOB
   @PJL ENTER LANGUAGE = TEXT
   <body>
   <FF>
   <ESC>%-12345X@PJL EOJ
   ```
5. Form-feed (`\f`) ejects the page; PJL EOJ closes the job

## Achievements

| Internal name | Display name | Tier |
|---|---|---|
| `wifi_printer_prank_first` | Toner Tagger | Bronze |
| `wifi_printer_prank_hit`   | Office Bard  | Silver |

## Caveats

- Some enterprise printers require authentication or restrict port 9100 — those will fail silently
- Some printers don't advertise via SSDP at all (they only do mDNS / IPP / DNS-SD); this screen won't find them
- A few stubborn printers ignore plain text and demand PCL/PostScript — those will print blank pages or queue garbage
- AirPrint-only / cloud-only printers (no LAN port 9100) won't print

## Ethics

Use only on networks you own or have permission to test. Wasting toner on someone else's printer is theft of physical resources in most jurisdictions, and IT teams notice. Keep messages short, friendly, and don't repeat — printer pranks are one-and-done.
