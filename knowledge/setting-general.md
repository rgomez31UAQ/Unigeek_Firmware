# General Settings

Device-wide preferences, reached from **Main menu → Settings**. Every change is written to `/unigeek/config` on device storage and persists across reboots. The list adapts to the board — hardware-dependent rows only appear when the board supports them, so two boards can show different Settings menus.

## Always available

These rows show on every board.

| Setting | What it does | Default |
|---|---|---|
| Name | Device name (text input) used for BLE/HID advertising and as the default Web/BLE File Manager label | `UniGeek` |
| Auto Display Off | Turns the screen off after an idle timeout to save power | On |
| Display Off | Idle timeout, in seconds, before the screen turns off | `10`s |
| Brightness | Backlight level | `70`% |
| Primary Color | Accent/theme color — Blue, Red, Green, Cyan, Purple, Brown, Orange, Violet, Navy | Blue |
| Web Password | Login password for the WiFi **Web File Manager** (text input) | `admin` |
| Serial File Manager | Enable/disable the USB-serial file manager — see below | Off |
| Screen Mirror | Enable/disable the USB-serial screen mirror + remote control — see below | Off |
| Pin Setting | Opens GPIO pin configuration for external modules — see [Pin Settings](setting-pin.md) | — |
| Device Status | Read-only hardware view: CPU frequency, free RAM, PSRAM, storage | — |
| About | Firmware version, build date, credits | — |

## Hardware-dependent

These rows appear only on boards with the matching hardware.

| Setting | What it does | Default | Shown on |
|---|---|---|---|
| Auto Power Off | Fully powers the device off after an idle timeout | On | Battery / power-managed boards: M5StickC Plus 1.1 & 2, Cardputer, Cardputer Adv, CoreS3, StickC S3, T-Lora Pager, T-Embed CC1101 |
| Power Off | Idle timeout, in seconds, before power-off | `60`s | Same as Auto Power Off |
| Volume | Speaker output level (hardware volume) | `75`% | Boards with a real speaker + volume control: Cardputer, Cardputer Adv, CoreS3, StickC S3, T-Lora Pager, T-Embed CC1101 |
| Navigation Sound | Beep on navigation / key presses | On | Any board with sound (the Volume boards **plus** M5StickC Plus 1.1 & 2, which have a buzzer) |
| Speaker Test | Plays Win / Lose / Notification / Beep to verify audio | — | Same as Navigation Sound |
| Navigation Mode | Switch Default vs Encoder/Joystick navigation | Default | M5StickC Plus 1.1 & 2 only |
| Screen Orientation | Rotate display Normal vs Flipped (also swaps UP/DOWN) | Normal | M5StickC Plus 1.1 & 2, StickC S3, CoreS3, CYD |
| Touch Guide | Replays the first-run touch gesture tutorial | — | Touch-only navigation boards: CoreS3, CYD |
| Navigation Overlay | Show/hide the on-screen touch navigation overlay | Show | Touch-only navigation boards: CoreS3, CYD |
| Touch Calibration | Three-point resistive-touch calibration | — | CYD touch boards |

> [!note]
> "Power Off" (full shutdown) is distinct from "Display Off" (screen only). Boards without a power-management chip — T-Display, T-Display S3, and the CYD/DIY USB-powered boards — only have the Display Off pair.

## Serial File Manager

Enables or disables the **USB-serial** file manager that powers the `https://unigeek.xid.run/app/files` page (connect over USB, no WiFi). It's **Off** by default.

- Turn it **On** to manage files over USB. While on it costs about **12 KB of internal SRAM** (the 8 KB protocol frame buffer plus the 4 KB serial RX FIFO) — kept off by default so no-PSRAM boards (M5StickC Plus, Cardputer, T-Display, …) don't lose that memory unless you need the feature.
- The toggle is **applied on the next restart** — flipping it resizes the serial RX buffer and frees/allocates the protocol core, which can't be done safely on a live serial link.
- This only affects the **USB-serial** transport. The **BLE** file manager is independent and started on demand from **Bluetooth → File Manager** — see [BLE File Manager](ble-file-manager.md).

> [!warn]
> If `https://unigeek.xid.run/app/files` won't connect over USB, check that **Settings → Serial File Manager** is **On** and that the device has been restarted since the last change.

## Screen Mirror

Enables or disables the **USB-serial screen mirror** that powers the `https://unigeek.xid.run/app/remote` page — see [Remote Access](remote-access.md). It's **Off** by default.

- Turn it **On** to mirror the device screen in a browser and drive it from your keyboard (and tap, on touch boards) over USB.
- Like Serial File Manager, it's **applied on the next restart** and only when on does it consume memory, so it stays off until you need it.

> [!warn]
> If `https://unigeek.xid.run/app/remote` shows nothing, check that **Settings → Screen Mirror** is **On** and that the device has been restarted since the last change.

## Related

- [Remote Access](remote-access.md) — mirror and control the device screen in a browser over USB serial
- [Pin Settings](setting-pin.md) — per-module GPIO pin configuration (also reachable from the Modules menu)
- [BLE File Manager](ble-file-manager.md) — the browser file manager over BLE and USB serial
- [Web File Manager](web-file-manager.md) — the WiFi-based file manager that uses the **Web Password** above
