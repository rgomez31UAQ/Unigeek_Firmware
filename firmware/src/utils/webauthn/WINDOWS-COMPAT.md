# WebAuthn Windows Compatibility — Working Notes

Status as of 2026-05-07: **Windows works on the regular `m5sticks3` build.**
macOS / Linux / Chrome / Firefox / Safari also continue to work as before.
Tested with Edge on Windows 10/11.

## Working configuration (2026-05-07)

1. Build + flash `m5sticks3` normally:
   ```
   pio run -e m5sticks3 -t upload
   ```
   Path-A (HID-only `m5sticks3_winfido`) was tried during bring-up but
   turned out unnecessary — the regular composite (CDC + HID) build is
   recognized by `webauthn.dll` once Windows has bound `HID-compliant fido`
   to the FIDO HID collection.

2. On the host, **disable USB selective suspend** for the device's port —
   easiest path is `tools/windows-fido-power.ps1` (run from elevated
   PowerShell). It writes `SelectiveSuspendEnabled=0` and
   `EnhancedPowerManagementEnabled=0` under every UniGeek instance Windows
   has registered. Manual alternative: Device Manager → the parent USB
   Root Hub → Properties → Power Management → uncheck "Allow the computer
   to turn off this device".

   Without this, Windows aggressively suspends the device after each CTAP
   exchange. The umount/remount cycle never recovers cleanly on
   arduino-esp32's USBHID stack, and the firmware-side watchdog (see
   below) takes over, producing a faint USB pulse on the WebAuthn screen.
   With selective suspend off, neither happens — the device stays
   continuously mounted.

3. Verified end-to-end on Edge: register + signin on **webauthn.io**
   (PIN-protected discoverable signin).

## Stuck-mount watchdog (firmware fallback)

`USBFidoUtil::poll()` runs an adaptive watchdog: if `tud_umount_cb` fired
(STOPPED) and no STARTED has come back within 300 ms (after recent
CTAPHID work) or 5 s (idle), it forces a soft re-attach via
`tud_disconnect/connect`. Windows then re-enumerates and TinyUSB
remounts. Cost: a constant ~0.2 Hz USB pulse while the WebAuthn screen
is active and selective suspend is on. Benefit: the device works without
any host-side configuration. Once `windows-fido-power.ps1` runs the
watchdog goes silent (no STOPPED → no kicks).

`USBFidoUtil::begin()` also advertises bmAttributes
`SELF_POWERED | REMOTE_WAKEUP` (matches Yubikey/SoloKey). In practice
Windows still chose to unmount instead of soft-suspend even with the
hint, so it doesn't replace the watchdog — kept as it's standards-
compliant and harmless.

The proper fix (path C below — TinyUSB-direct, like pico-fido) would
eliminate the umount churn entirely. Left for follow-up.

## Original symptom (before bring-up)

On Windows, `webauthn.dll` would not engage with UniGeek as a security
key. Browsers fell back to "no security key found" or hung. macOS/Linux
on the same hardware enrolled passkeys without complaint.

## Why (initial hypothesis)

The arduino-esp32 USB stack on ESP32-S3 builds a composite USB device:

```
Interface 0: CDC Communication   (HWCDC — Serial)
Interface 1: CDC Data
Interface 2: HID                 (FIDO, our descriptor)
```

Initial reading of FIDO-on-ESP32 prior art (SoloKeys, pico-fido,
ChameleonUltra) suggested Windows webauthn.dll prefers either a
HID-only device or a dedicated FIDO HID interface. We tried a
HID-only variant (`m5sticks3_winfido`, `ARDUINO_USB_CDC_ON_BOOT=0`).
That confirmed the binding worked, but later testing showed the
regular composite build also works once Windows has bound
`HID-compliant fido` to the FIDO collection — the real blocker
turned out to be selective-suspend lifecycle, not the descriptor.
The `winfido` variant has been removed.

## Remaining fix paths (left for follow-up)

### Path C — Bypass arduino-esp32 USB entirely

Drop `USBHID` / `USB.begin()`. Call `tinyusb_driver_install()`
directly and provide our own `tud_descriptor_*` callbacks. Full
control over the device + configuration descriptors. Mirrors
`pico-fido` architecture.

Eliminates the umount-on-suspend issue at the source — no need for
the watchdog or the `windows-fido-power.ps1` workaround. Other USB
profiles (kbd, mouse) need similar treatment OR run alongside our
custom TinyUSB stack via descriptor reconciliation.

**Estimate:** 3–5 days. Highest reward, highest risk.

## Windows test checklist

When testing on a Windows machine, capture:

- [ ] Output of `chrome://device-log` (or `edge://device-log`) filtered
      to `usb` category during a register attempt
- [ ] Device Manager → Human Interface Devices → properties → Details
      tab → Hardware Ids (should show `HID\VID_303A&PID_1001`)
- [ ] Device Manager → ditto → Compatible Ids (should include
      `HID_DEVICE_UP:F1D0_U:0001`)
- [ ] `USBView.exe` dump of the full descriptor tree
- [ ] Optional: register attempt with `webauthn.io` and check what
      fails (browser console)

For host-side diagnosis without the browser:

- `scripts/webauthn/win_probe.py` — Windows-native CTAPHID probe via
  python-fido2. Run from **elevated** PowerShell — Windows ACL on
  FIDO HID blocks user-mode `CreateFile` since Win10 1903.

## Files involved

- `firmware/src/utils/webauthn/USBFidoUtil.cpp` — `USBHID` integration,
  watchdog, bmAttributes setup. Descriptor (`kFidoReportDescriptor`)
  matches Yubikey 5.
- `firmware/src/utils/webauthn/UsbProfile.cpp` — single-HID-interface
  arbiter. Prevents kbd/mouse from co-existing on the same boot.
- `tools/windows-fido-power.ps1` — host-side selective-suspend disable.
- `tools/windows-fido-power.md` — script README + background.

## Reference implementations

- **pico-fido** (`../pico-fido`): TinyUSB-direct FIDO HID for Pi Pico W.
  See `src/usb/usb_descriptors.c` for the descriptor template Windows
  accepts. Reference for path C.
- **SoloKey 2**: `tinyusb_config.h` + ST32-Cube descriptors. HID-only
  composite device, no CDC.
- **ChameleonUltra**: `Application/src/usb_main.c` — sets CDC AND HID,
  works on Windows. Worth diffing the descriptor bytes against ours.
