# A30 Settings UI Policy

This document defines the initial policy for A30-specific settings in the
plumOS frontend.

The goal is not to copy the stock frontend design by default. First, plumOS
observes the current device state safely, validates the right backend for each
setting, then connects that backend to plumOS UI and services.

## Basic Policy

- Keep plumOS frontend settings separate from A30 device settings.
- Store plumOS frontend settings in
  `/mnt/SDCARD/plumos/config/frontend/settings.json`.
- Use `/config/system.json` and runtime status files as the first A30 device
  observation sources.
- Keep the first UI as a read-only inventory. Do not write A30 settings yet.
- Confirm the reason and risk before adopting a stock behavior.
- Never expose Wi-Fi SSID, PSK, or `/config/wpa_supplicant.conf` contents in
  git, logs, or UI.

## Current Values In Controller UI

Opening Settings from the START menu in `plumos-controller-ui` shows plumOS
frontend settings, theme state, and these A30 entries:

- `A30 Policy`: read-only inventory
- `A30 Write Policy`: write support deferred until backend validation
- `A30 Config`: read status for `/config/system.json`
- `A30 Volume`: `vol`, `mute`, `bgmvol`
- `A30 Volume Backend`: candidate based on whether `amixer` exists
- `A30 Brightness`: `brightness`, `lumination`
- `A30 Display Color`: `contrast`, `hue`, `saturation`
- `A30 Brightness Backend`: backlight/lcd sysfs candidate
- `A30 Wi-Fi Config`: `wifi` from `/config/system.json`
- `A30 Wi-Fi Runtime`: `wpa_state`, IP, RSSI, link speed, frequency
- `A30 Keymap`: `keymap` from `/config/system.json`
- `A30 Input Event`: `/dev/input/event*` found from `gpio-keys-polled`
- `A30 Language`: `language` from `/config/system.json`
- `A30 Stock Theme`: stock theme path
- `A30 CPU Mode`: `cpufreq` from `/config/system.json`

Wi-Fi runtime reads only selected keys from `/tmp/wpa_status.txt`. It does not
read SSID or PSK.

## Brightness

On the observed A30, `/sys/class/backlight` does not expose usable brightness
files. `/config/system.json` contains `brightness`, `lumination`, `contrast`,
`hue`, and `saturation`.

For now, the UI only displays current values. Write support should wait until
we confirm which kernel, sysfs, or API path the stock frontend uses.

## Volume

`/config/system.json` contains `vol`, `mute`, and `bgmvol`. The device also has
`amixer`, so direct ALSA mixer control may be possible.

The mixer control mapping is not validated yet. Before write support, inspect
`amixer contents` and verify which controls change the actual output volume.

## Wi-Fi

Runtime state is available in `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, and
`/proc/net/wireless`. Connection settings live in `/config/wpa_supplicant.conf`,
which is sensitive and must not be displayed directly.

Write support should meet at least these requirements:

- Reproduce the existing Wi-Fi power sequence.
- Back up the settings file first.
- Write through a temporary file followed by atomic rename.
- Confirm DHCP and status updates after service restart.
- Restore the previous settings on failure.

## Keymap/Input

The stock frontend uses `keymap` in `/config/system.json` and `keymon`. The
plumOS controller prototype can also read `/dev/input/event*` for
`gpio-keys-polled` directly.

Next, compare keeping stock `keymon` with direct SDL/input-event handling. If
direct input is stable, the frontend should prefer plumOS-owned input mapping.

## Conditions For Write-Enabled UI

A30 settings controls should become write-enabled only after these conditions
are met:

- Read sources and write targets are clearly separated.
- A pre-change backup is always created.
- Atomic write and `sync` are used.
- Runtime application can be verified after the change.
- Rollback is available on failure.
- SSH recovery remains possible during development.

Until then, the A button in Settings remains an edit preview.
