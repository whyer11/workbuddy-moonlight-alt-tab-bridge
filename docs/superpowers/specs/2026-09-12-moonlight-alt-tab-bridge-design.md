# Moonlight Alt-Tab Bridge Design

## Goal

When Moonlight is the foreground app on the Xiaomi Pad, `Alt+Tab` must switch the foreground application on the streamed Windows desktop without opening Android multitasking or pausing the Sunshine session. Outside Moonlight, Android keyboard behavior must remain unchanged.

## Evidence and selected approach

Three approaches were evaluated:

1. HyperOS Workstation mode or Android screen pinning prevents Moonlight from losing focus, but the `Alt+Tab` event is swallowed and never reaches Windows.
2. LSPosed modules can hook Xiaomi's framework keyboard policy, but require root, are ROM-version dependent, and are not validated for this Android 17 / HyperOS 4 device.
3. A small local bridge can consume only Moonlight's `Alt+Tab` and send an authenticated command over the existing Tailscale link to the Sunshine host. This is the selected approach because it is rootless, does not replace Moonlight, is automatic, and has a narrow security boundary.

## Architecture

The solution has two components:

- `android-bridge`: a separate Android application containing an accessibility service. It tracks the active package. When `com.limelight` is active and it receives `KEYCODE_TAB` with an Alt modifier, it consumes the Tab down/up events and sends one `ALT_TAB` command on key-down. All keys and all other applications pass through unchanged.
- `windows-host`: an invisible per-user Windows process. It listens only on the PC's Tailscale IPv4 address and validates each command before using `SendInput` to press and release `Alt+Tab` in the interactive Windows session.

The existing official Moonlight installation, pairing data, Sunshine configuration, display driver, and stream settings are not modified.

## Wire protocol and security

- Transport: UDP over Tailscale, PC `100.81.73.56`, Pad `100.74.136.33`, high unassigned port `42837`.
- Canonical signed value: `v1|timestamp_ms|nonce_hex|ALT_TAB`.
- Authentication: HMAC-SHA-256 with a random 32-byte key generated during installation.
- Replay defense: reject timestamps outside a 10-second window and reject a nonce already accepted during that window.
- Network restriction: the Windows listener binds only to `100.81.73.56`; the firewall rule allows UDP 42837 only from `100.74.136.33`.
- Secret handling: the key is written to a user-only Windows configuration file and compiled into the locally built APK. It is never printed in logs or the final report.
- Logs contain timestamps and accept/reject reasons only, never arbitrary keystrokes or secrets.

## Android behavior

The accessibility service requests key-event filtering and window-state events. It keeps the last active package name and acts only when it is exactly `com.limelight`.

For an Alt-modified Tab press:

1. Consume Tab down so HyperOS cannot open multitasking.
2. Send one authenticated `ALT_TAB` datagram asynchronously.
3. Consume the matching Tab up.
4. Leave Alt events unchanged; the Windows host emits a complete atomic `Alt+Tab`, so the result does not depend on Moonlight forwarding the Alt state.

If Tailscale or the PC helper is unavailable, Android still consumes that one shortcut so Moonlight is not paused. No other key is affected.

## Windows behavior

The host starts at user logon in the interactive session and runs without a visible console window. It validates the sender IP, payload format, HMAC, timestamp, and nonce. Only the exact command `ALT_TAB` is accepted. Valid commands invoke a short `SendInput` sequence: Alt down, Tab down, Tab up, Alt up.

The host process and scheduled task are user-scoped. The firewall rule is the only system-scoped change.

## Failure handling and rollback

- Invalid, stale, replayed, or unknown commands are ignored and logged.
- Network errors keep the host alive and return it to the receive loop.
- Disabling the accessibility service immediately restores normal Android shortcut behavior.
- Full rollback removes the APK, scheduled task, firewall rule, and project directory; it does not touch Moonlight or Sunshine.

## Verification

Automated tests cover:

- Android shortcut decision logic: only Alt-modified Tab in Moonlight is consumed and emits one command.
- Windows message validation: valid HMAC acceptance plus wrong-MAC, stale-timestamp, unknown-command, and replay rejection.
- Windows key injection behind an injectable interface so validation tests do not switch windows.

The final device test must be performed in normal tablet mode with a live 3408x2272@144 Moonlight session:

1. Record the Android resumed activity and Windows foreground title.
2. Send a real Alt+Tab combination from the Pad keyboard path.
3. Verify Android remains on `com.limelight/.Game`.
4. Verify Sunshine still reports one active session and no disconnect.
5. Verify the Windows foreground title changes.
6. Verify Alt+Tab outside Moonlight still opens Android multitasking normally.

