# Download

One-stop over-the-air downloader. **WiFi → Network → Download** lists five categories — each one fetches a curated set of files from a public GitHub mirror and writes them directly to device storage. Useful when you don't have an SD-card adapter handy and want to pull samples, IR remotes, payloads, web assets, or Lua scripts straight onto the device.

> [!note]
> Every category requires an active WiFi connection. The screen aborts with a `WiFi not connected` toast if you haven't joined a network from **WiFi → Network** first.

## Menu

| Item | Source repo | Destination | Notes |
|------|-------------|-------------|-------|
| **Web File Manager** | `lshaf/puteros-file-manager` | `/unigeek/web/file_manager/` | Sublabel shows installed version (`v` + first 7 chars of the SHA in `version.txt`). |
| **Firmware Sample Files** | `lshaf/unigeek` (`manifest/sdcard.txt`) | `/unigeek/...` | Whole-tree refresh of bundled samples — portals, DuckyScript, QR/barcode samples, rockyou_mini, DNS spoof config. |
| **Infrared Files** | [Flipper-IRDB](https://github.com/Flipper-XFW/Flipper-IRDB) | `/unigeek/ir/downloads/<category>/` | Browse by category (TVs, ACs, Fans, Projectors, …). |
| **BadUSB Scripts** | `lshaf/badusb-collection` | `/unigeek/hid/duckyscript/<os>/` | Browse by OS → category → script. |
| **Lua Scripts** | [lshaf/unigeek-lua](https://github.com/lshaf/unigeek-lua) | `/unigeek/lua/<path>/` | Hierarchical browser driven by a single `map.txt`. |

## Web File Manager

Pulls the latest `index.html`, `index.css`, `index.js`, plus a `version.txt` containing the upstream commit SHA. The sublabel on the menu line reads `v<sha7>` so you can spot when an update is available — compare against the current upstream commit on GitHub before re-downloading.

Used by **WiFi → Network → Web File Manager**; if you've never run that screen the assets won't exist on disk and the menu shows `Not installed`. Pulling this entry is the only way to install or update the browser UI.

## Firmware Sample Files

Downloads everything listed in `manifest/sdcard.txt` from the firmware repo's `main` branch — including:

- Portal HTML/CSS/JS for **Access Point**, **Evil Twin**, **Karma Captive**, **Karma EAPOL**, and **Rogue DNS**
- DuckyScript samples (hello world, reverse shell, WiFi password grab, rickroll, disable Defender)
- QR code samples (WiFi credentials, contact card, URL)
- DNS spoofing config (`/unigeek/wifi/dnsspoof.conf`)
- `rockyou_mini.txt` password wordlist for **EAPOL Brute Force**

Files overwrite their existing copies — useful after wiping `/unigeek/` or moving to a fresh SD card. A progress bar tracks the manifest line count.

## Infrared Files

Two-level browse of [Flipper-IRDB](https://github.com/Flipper-XFW/Flipper-IRDB):

1. Pick a **category** (e.g. `TVs`, `ACs`, `Projectors`) from the `manifest/ir/categories.txt` list — each line is `folder|Display Name`.
2. The category's per-file manifest (`manifest/ir/cat_<folder>.txt`) drives a bulk download of every `.ir` file in that category into `/unigeek/ir/downloads/<folder>/`.

Files are immediately usable from **Modules → IR Remote → Send**. The format is shared with Flipper Zero and Bruce, so anything in the IRDB just works.

## BadUSB Scripts

Three-level browse of `lshaf/badusb-collection`:

1. **OS** — pick the target OS folder.
2. **Category** — pick a category within that OS.
3. **Script** — pick one or more `.txt` files; each downloads to `/unigeek/hid/duckyscript/<os>/<filename>`.

Files are then runnable from **HID → Ducky Script**.

## Lua Scripts

Single-file driven hierarchical browser of [lshaf/unigeek-lua](https://github.com/lshaf/unigeek-lua):

- A single `map.txt` at the repo root lists every script path, one per line — e.g. `utility/morse/generator.lua`.
- The map is fetched once on entry and held in memory. Each navigation level is derived locally from the cached lines, so descending into a folder is instant after the first fetch.
- Folders are listed before files at every level. Selecting a `.lua` downloads that one file to `/unigeek/lua/<full/path>/<filename>.lua`; selecting a folder enters it.
- BACK in a sub-folder pops one level; BACK at the root exits to the Download menu.

Scripts run from **Lua Runner** without any further setup.

> [!tip]
> Comments are allowed in `map.txt` — lines starting with `#` and blank lines are skipped. UTF-8 BOMs on the first line are tolerated. If you fork the repo, point a custom `LUA_BASE_URL` / `LUA_MAP_URL` (via firmware rebuild) at your own raw GitHub URL.

> [!warn]
> The map is fetched with `Cache-Control: no-cache` so you always get fresh data, but the underlying download is HTTPS via `setInsecure()` — no certificate validation is performed. Don't ingest Lua from untrusted mirrors; the runner has filesystem and WiFi access.

## Storage Summary

```
/unigeek/web/file_manager/             Web File Manager UI assets + version.txt
/unigeek/ir/downloads/<category>/      Flipper-format .ir files
/unigeek/hid/duckyscript/<os>/         DuckyScript payloads
/unigeek/lua/<path>/                   Lua 5.1 scripts
/unigeek/web/portals/                  AP / Evil Twin / Karma captive portal HTML/CSS/JS
/unigeek/wifi/dnsspoof.conf            DNS spoof rule file
/unigeek/utility/passwords/            rockyou_mini.txt wordlist
/unigeek/qrcode/                       QR code samples
/unigeek/barcode/                      Barcode samples
```

## Achievement

| Achievement | Tier | How to unlock |
|-------------|------|---------------|
| Script Kiddie | Bronze | Download any `.lua` file from the unigeek-lua repo. |
