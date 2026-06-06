# BLE File Manager

Manage the on-device storage of a UniGeek from a web browser **wirelessly over Bluetooth LE** — no WiFi network, no IP address, and no password. The device exposes a file-manager protocol over the Nordic UART Service (NUS); the companion web page drives it via Web Bluetooth.

The **same web page** (`https://unigeek.xid.run/app/files/`) also connects over **USB serial** (Web Serial) when the board is plugged in — pick whichever transport is available.

## Start it on the device

1. Go to **Bluetooth > File Manager**
2. The device begins advertising as **`UniGeek FM`** and shows the connection URL
3. The screen status reads **Advertising** until a browser connects, then **Connected**
4. Press **BACK** to stop advertising and free the radio

> [!note]
> The on-device screen only advertises and shows status — all browsing, viewing, and editing happens in the browser. The device just serves files over the link.

## Connect from the browser

1. Open `https://unigeek.xid.run/app/files/` in a browser that supports Web Bluetooth (Chrome / Edge on desktop or Android)
2. Click **Connect** and pick **`UniGeek FM`** from the pairing dialog
3. Browse the device storage once connected

For a wired connection instead, plug the board in over USB and the same page connects over Web Serial — no on-device screen needed, but the **Serial File Manager** must be enabled (see below).

> [!warn]
> The USB-serial transport is gated by **Settings → Serial File Manager**. It's **Off** by default (to save memory on no-PSRAM boards), so the device won't respond over USB until you switch it on **and restart** — the toggle is applied at boot. The BLE transport is unaffected: it's started on demand from **Bluetooth → File Manager**.

## What you can do

- **Browse** directories and files on device storage
- **View** text files, with a **hex** view for binaries
- **Edit** text files in-browser and save back to the device
- **Image preview** for supported image files
- **Upload** by drag-and-drop onto the page
- **Download**, rename, and delete files

## Transports at a glance

| | BLE File Manager | Web File Manager | USB serial |
|---|---|---|---|
| Link | Bluetooth LE (NUS) | WiFi (HTTP :8080) | USB (Web Serial) |
| Page | `https://unigeek.xid.run/app/files/` | served by device | `https://unigeek.xid.run/app/files/` |
| Password | none | required | none |
| Range | nearby (BLE) | same network | cable |

> [!tip]
> BLE transfers are framed with flow control, so large files move reliably but slower than USB serial. For bulk transfers, prefer the cable; for quick edits without plugging in, use BLE.

> [!warn]
> Anyone in BLE range who connects to `UniGeek FM` can read and write device storage while the screen is active — there is no password on this link. Stop the screen (BACK) when you're done.
