# Remote Access

Mirror a UniGeek's screen in your browser and drive the device from your keyboard — navigate menus, type into inputs, and tap touch screens — all **over USB serial**, with no WiFi, no IP address, and no password. The device streams what it draws out the serial port; the companion web page renders it and sends your input back.

Open it at `https://unigeek.xid.run/app/remote` (also reachable from the site's **Apps → Remote Access** menu, next to File Manager).

## Enable it on the device

Remote Access rides on the **Screen Mirror** serial service, which is **off by default** to save memory.

1. Go to **Settings → Screen Mirror** and switch it **On**
2. **Restart** the device — the toggle is applied at boot
3. Plug the board into your computer over USB

> [!note]
> There is no on-device screen to open. Screen Mirror is purely a setting that turns on the serial stream; everything else happens in the browser.

## Connect from the browser

1. Open `https://unigeek.xid.run/app/remote` in a browser that supports **Web Serial** (Chrome or Edge on desktop)
2. Click **Connect device** and pick the board's serial port
3. The stream starts automatically — the device screen appears and updates as you use it

If nothing appears within a few seconds, the page tells you to turn on **Screen Mirror** in Settings and reconnect — that almost always means the service is still off.

## Controls

Input adapts to what the board has (reported by the device when the stream starts):

| Key / action | Effect |
|---|---|
| Arrow keys | Navigate (up / down / left / right) |
| Esc | Back |
| Enter | Select (on keyboard boards, **types** a newline instead) |
| Enter (hold) | Long-press (non-keyboard boards) |
| Space | Press (non-keyboard boards) |
| Ctrl/⌘ + Enter | Select — the workaround on keyboard boards |
| Ctrl/⌘ + Backspace | Back — the workaround on keyboard boards |
| Type letters / Backspace | Enter text into on-device inputs (keyboard boards) |
| Click the screen | Touch tap at that point (touch boards) |

> [!tip]
> On keyboard boards (Cardputer, T-Lora Pager) plain **Enter** and **Backspace** are sent as keystrokes so you can type into fields. Use **Ctrl/⌘+Enter** to select a menu item and **Ctrl/⌘+Backspace** to go back.

## How it works

The device captures every draw as it renders and forwards it out the serial port as small region frames (it never needs to read the panel back, so it works even on write-only displays). The browser paints those regions onto a canvas; your key presses and clicks go back as input events the firmware injects into its normal navigation, keyboard, and touch paths — so the remote behaves exactly like using the device by hand.

> [!note]
> Streaming is lightweight (no full-screen framebuffer) so it stays smooth even on large screens. The trade-off: a few effects drawn straight to the panel (rather than through a sprite) may not mirror. Everything rendered the usual way shows up.

> [!warn]
> Anyone with physical access to the USB cable can view and control the device while Screen Mirror is on. Turn the setting back off (and restart) when you don't need it.
