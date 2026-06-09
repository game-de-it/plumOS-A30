# A30 Settings UI Policy

This document defines the initial policy for A30-specific settings in the
plumOS frontend.

The goal is not to copy the stock frontend design by default. First, plumOS
observes the current device state safely, validates the right backend for each
setting, then connects that backend to plumOS UI and services.

## Basic Policy

- Keep persistent plumOS settings under `/mnt/SDCARD/plumos/`.
- Follow `docs/plumos-config-layout.md` for the config layout.
- Do not use stockOS-owned `/config/system.json` as normal plumOS configuration.
- Make A30 write-enabled controls available only for entries that have backup,
  atomic write, `sync`, and a recovery policy.
- Confirm the reason and risk before adopting a stock behavior.
- Never expose Wi-Fi SSID, PSK, or `/config/wpa_supplicant.conf` contents in
  git, logs, or UI.

## Current Values In Controller UI

Opening System Settings from the START menu in `plumos-controller-ui` shows the
settings users should recognize as adjustment targets. As of 2026-06-09, the UI
reads and writes `/mnt/SDCARD/plumos/config/system/settings.json`; it does not
touch stockOS `/config/system.json`. It still does not directly apply
unvalidated mixer or sysfs runtime backends.

- `Volume`: `volume`; Left/Right changes `0..20`. Later
  this should track the physical volume buttons
- `Brightness`: `brightness`; Left/Right changes `0..10`. Later this should
  track a hotkey such as START + volume
- `Lumination`: `lumination`; Left/Right changes `0..10`
- `Display Color`: A opens a subpage where `Contrast`, `Hue`, and `Saturation`
  each change in the `0..20` range
- `Language`: `language`; Left/Right selects `English`, `Japanese`, `Chinese`,
  `Traditional Chinese`, `Korean`, `Spanish`, or `Portuguese`
- `Theme`: theme setting candidate for graphical mode; read-only until candidate
  names and paths are defined safely
- `INFORMATION`: read-only subpage for current values, backend state, and policy

The `INFORMATION` subpage owns these read-only entries:

- `Device Model`: fixed `Miyoo A30`
- `Linux Kernel`: `/proc/sys/kernel/osrelease`
- `SD Card`: free/total capacity from `statvfs()`
- `plumOS System Config`: read status for
  `/mnt/SDCARD/plumos/config/system/settings.json`
- `Input Device`: `/dev/input/event*` found from `gpio-keys-polled`
- `Theme Source`: plumOS theme id
- `Audio Backend`: plumOS config only
- `Display Backend`: plumOS config only
- `Write Policy`: save under plumOS only; stockOS remains untouched

Network Settings owns these entries:

- `Wi-Fi`: plumOS runtime
- `Connection`: `wpa_state` from `/tmp/wpa_status.txt`
- `IP Address`: `ip_address` from `/tmp/wpa_status.txt`
- `Signal`: `RSSI` from `/tmp/wpa_status.txt`
- `Link Speed`: `LINKSPEED` from `/tmp/wpa_status.txt`
- `Frequency`: `FREQUENCY` from `/tmp/wpa_status.txt`
- `SSH`: plumOS remote access path. Current value is Dropbear port 2222
- `Status Source`: runtime status source
- `Config Source`: plumOS network runtime
- `Credentials`: `hidden`
- `Run Network Recovery`: A runs Wi-Fi, DHCP, and SSH recovery
- `Write Policy`: read-only until a safe Wi-Fi editor exists

Pressing A on Wi-Fi status, IP, or signal rows does not run recovery.
Performance Settings owns launcher/core-profile CPU policy. As of 2026-06-09,
the user can choose a system and change `CPU freq` and `CPU Cores` with
Left/Right. `CPU freq` exposes only fixed `648/816/1200/1344 MHz` values, and
unpredictable `keep` is removed. Saves use the existing `plumos-text-ui core
system ... --cpu --freq --cores` flow and write system overrides to
`/mnt/SDCARD/plumos/state/frontend/core-overrides.json`. `Reset to Default`
falls back to the `systems.json` `648 MHz` / `2 cores` plumOS defaults. The
controller UI also sets `userspace 648 MHz` / `2 cores` as the FE runtime
baseline on startup.

Wi-Fi runtime reads only selected keys from `/tmp/wpa_status.txt`. It does not
read SSID or PSK.

## Brightness

On the observed A30, `/sys/class/backlight` does not expose usable brightness
files. plumOS stores `brightness`, `lumination`, `contrast`, `hue`, and
`saturation` in `/mnt/SDCARD/plumos/config/system/settings.json`.

The UI updates `brightness`, `lumination`, `contrast`, `hue`, and `saturation`
in plumOS system settings using backed-up atomic writes. Direct runtime backend
application still waits until we confirm which kernel, sysfs, or API path the
stock frontend uses.

## Volume

plumOS stores `volume` in `/mnt/SDCARD/plumos/config/system/settings.json`. The
device also has `amixer`, so direct ALSA mixer control may be possible.

The UI updates `volume` in plumOS system settings using backed-up atomic writes. The
mixer control mapping is not validated yet. Before connecting physical volume
buttons or immediate volume application, inspect `amixer contents` and verify
which controls change the actual output volume.

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

The plumOS controller prototype can read `/dev/input/event*` for
`gpio-keys-polled` directly.

`plumos-input-compare` confirmed that `/dev/input/event3` can be opened and
polled non-exclusively even while stock `keymon` and stock `MainUI` are running.
The initial policy is to keep stock `keymon`, while the frontend prefers
plumOS-owned input mapping.

## Conditions For Write-Enabled UI

A30 settings controls should become write-enabled only after these conditions
are met:

- Read sources and write targets are clearly separated.
- A pre-change backup is always created.
- Atomic write and `sync` are used.
- Runtime application can be verified after the change.
- Rollback is available on failure.
- SSH recovery remains possible during development.

Only limited plumOS-owned keys that meet these requirements are write-enabled.
Direct unvalidated runtime backends should be added later.

Until then, the A button in Settings remains an edit preview.
