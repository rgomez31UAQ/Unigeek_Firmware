# UART Terminal

A serial terminal that communicates with external hardware over configurable GPIO pins. Set baud rate and RX/TX GPIOs, send commands in text or raw hex, receive incoming data in the background while you type, and optionally save the full session log to storage.

> [!note]
> Requires external hardware wired to the device's GPIO pins. Default pins are taken from the **Pin Configuration** settings (Grove SDA/SCL). Change them in the setup screen before starting.

## Setup

Open **Utility → UART Terminal** and configure:

| Field | Default | Notes |
|-------|---------|-------|
| Baud Rate | 115200 | 9600 / 19200 / 38400 / 57600 / 115200 / 230400 |
| RX GPIO | Grove SDA pin | Enter `0` to disable RX |
| TX GPIO | Grove SCL pin | Enter `0` to disable TX |
| Log Mode | Off | Off / File / Stream AP / Stream Network |
| (mode-specific) | — | Filename, AP name, or scan-and-pick a known network — depends on Log Mode |

Select **Start** to open the terminal.

### Log Mode

The terminal can persist or stream the running session. Pick one in **Log Mode**:

| Mode | What it does |
|------|--------------|
| **Off** | No logging — terminal output is on-screen only. |
| **File** | Buffer up to 200 lines in memory; on exit, flush to `/unigeek/utility/uart/<filename>.log`. Filename auto-suggests `YYYYMMDD_HHMMSS` when NTP time is synced. |
| **Stream AP** | Bring up a SoftAP (default SSID `UartStream`) and a Telnet server on TCP port 23. Every connected client (up to 4) receives every received and sent line in real time. Connect with `nc <ap-ip> 23` or any Telnet client. |
| **Stream Network** | Join a known WiFi network and run the same TCP-23 server on the device's LAN address. Useful when you want the host you're already on (laptop, lab PC) to scrape the stream. |

The selected Log Mode and connection details are printed to the on-screen log when the terminal starts (e.g. `Log Mode: Stream AP` then `Stream: nc 192.168.4.1:23`) so you don't need to memorise them up front.

## Terminal Controls

| Input | Action |
|-------|--------|
| Press (Select) | Open send-command dialog |
| UP or DOWN | Toggle hex / string send mode |
| Back | Exit terminal (saves log if enabled) |

### Send Modes

**String mode** (default) — type a text command, the terminal sends it followed by `\r\n`. Incoming and outgoing lines are shown as readable text.

**Hex mode** — enter pairs of hex digits separated by spaces (e.g. `A5 3F 00`). The bytes are sent as raw binary. Echo is prefixed with `[H]`. Useful for binary protocols, AT commands with control bytes, or low-level hardware debugging.

The bottom status bar shows two badges at all times: send mode (`HEX` yellow / `STR` grey) and log mode (`STRM` green when Stream AP/Network is up, `LOG` when File logging is active, blank when Off).

## Receiving Data

Incoming bytes are buffered line by line (split on `\n` or at 58 characters). Each line appears in the scrolling log in white. The screen refreshes every 200 ms.

## Session Logging

See **Log Mode** above. **File** mode buffers up to 200 lines in memory and writes them to `/unigeek/utility/uart/<filename>.log` on exit. **Stream AP** / **Stream Network** push every line out over TCP-23 in real time without writing to flash.

> [!tip]
> If you set a filename when NTP time is synced, the default suggestion is a timestamp (`YYYYMMDD_HHMMSS`) so each session gets a unique name automatically.

## Achievements

| Achievement | Tier |
|-------------|------|
| Wire Tapper | Bronze |
| Signal Caught | Silver |
| Terminal Logger | Bronze |
| Terminal Operator | Bronze |
