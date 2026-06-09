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
- Never expose Wi-Fi PSK or `/config/wpa_supplicant.conf` contents in git or
  logs. The PSK is visible only while the user is entering it in `Connect Wi-Fi`;
  the connection backend still receives it through a temporary file. SSIDs are
  shown only in the explicit `Connect Wi-Fi` selection screen and must not be
  written to logs or git.

## Current Values In Controller UI

Opening System Settings from the START menu in `plumos-controller-ui` shows the
settings users should recognize as adjustment targets. As of 2026-06-09, the UI
reads and writes `/mnt/SDCARD/plumos/config/system/settings.json`; it does not
touch stockOS `/config/system.json`. Except for `Language` and `Theme`, saves
are applied immediately to the A30 runtime backend.

- `Volume`: `volume`; Left/Right changes `0..20`. Later
  this should track the physical volume buttons
- `Brightness`: `brightness`; Left/Right changes `1..20` and applies the mapped
  RAW value to `/sys/devices/virtual/disp/disp/attr/lcdbl`. Later this should
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
- `Audio Backend`: detected mixer backend. On A30 this is
  `Soft Volume Master (amixer)`
- `Display Backend`: detected display backend. On A30 this is
  `disp attr lcdbl/enhance`
- `Write Policy`: save under plumOS only; stockOS remains untouched

The first Network Settings layer owns only actionable items:

- `Wi-Fi`: checkbox. A toggles the runtime on/off. ON runs
  `plumos-network-rescue`; OFF runs `plumos-network-control --wifi off` and
  does not edit saved credentials.
- `Connect Wi-Fi`: A opens the connection flow: SSID scan, password entry,
  DHCP, one default-gateway ping when available, then an IP-address result
  screen. The PSK is passed to the backend through a temporary file and is not
  logged.
- `NW Service`: opens the file-transfer service subpage for FTP/SFTP/Samba/USB
  Disk Mode.
- `Run Network Recovery`: A runs Wi-Fi, DHCP, and SSH recovery.
- `INFORMATION`: opens the read-only network information subpage.

The `NW Service` subpage owns these actions:

- `FTP`: checkbox. A toggles the BusyBox FTP service. The shared root is
  `/mnt/SDCARD/`.
- `SFTP`: checkbox. A toggles the Dropbear SFTP subsystem. Turning it OFF does
  not stop SSH shell access.
- `Samba`: checkbox. A toggles the `SDCARD` SMB share. If Windows/macOS asks for
  credentials, use `plumos` / `plumos`.
- `USB Disk Mode`: A opens a confirmation screen; pressing A again exposes the
  SD card partition to the PC as USB Mass Storage.

The `INFORMATION` subpage owns these read-only entries:

- `Connection`: `wpa_state` from `/tmp/wpa_status.txt`
- `IP Address`: `ip_address` from `/tmp/wpa_status.txt`
- `Signal`: `RSSI` from `/tmp/wpa_status.txt`
- `Link Speed`: `LINKSPEED` from `/tmp/wpa_status.txt`
- `Frequency`: `FREQUENCY` from `/tmp/wpa_status.txt`
- `SSH`: plumOS remote access path. Current value is Dropbear port 2222
- `FTP`: `plumos-network-services status ftp`
- `SFTP`: `plumos-network-services status sftp`
- `Samba`: `plumos-network-services status samba`

Pressing A on Wi-Fi status, IP, signal, or other information rows does not run recovery.
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
in plumOS system settings using backed-up atomic writes, then applies them to
`/sys/devices/virtual/disp/disp/attr/lcdbl` and
`/sys/devices/virtual/disp/disp/attr/enhance`. User-facing `brightness 1..20`
maps to the following RAW lcdbl values based on visual testing:

| brightness | lcdbl RAW |
|---:|---:|
| 1 | 2 |
| 2 | 3 |
| 3 | 4 |
| 4 | 5 |
| 5 | 6 |
| 6 | 7 |
| 7 | 8 |
| 8 | 9 |
| 9 | 10 |
| 10 | 26 |
| 11 | 43 |
| 12 | 59 |
| 13 | 75 |
| 14 | 92 |
| 15 | 108 |
| 16 | 125 |
| 17 | 141 |
| 18 | 157 |
| 19 | 174 |
| 20 | 190 |

## Volume

plumOS stores `volume` in `/mnt/SDCARD/plumos/config/system/settings.json`.
The UI updates `volume` using backed-up atomic writes and then applies it with
`amixer cset iface=MIXER,name='Soft Volume Master'`. `volume 0..20` maps to
mixer values `0..255`. Physical volume button tracking is a separate task.

## Wi-Fi

Runtime state is available in `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, and
`/proc/net/wireless`. Connection settings live in `/config/wpa_supplicant.conf`,
which is sensitive and must not be displayed directly.

`Connect Wi-Fi` updates `/config/wpa_supplicant.conf` only after a successful
connection because the A30 stock `wpa_supplicant` init reads that path. plumOS
creates a pre-change backup under `/mnt/SDCARD/plumos/backups/network/` and
keeps a successful copy under
`/mnt/SDCARD/plumos/config/network/wpa_supplicant.conf`. Connection attempts
start `wpa_supplicant` from a `/tmp` candidate file, acquire an IP address with
DHCP, and run one ping to the default gateway when one exists. If IP acquisition
fails, plumOS restores the previous `/config/wpa_supplicant.conf` and tries to
restart the previous configuration.

The connection flow order is:

- Scan SSIDs.
- Enter password.
- Start `wpa_supplicant`.
- Acquire an IP address with DHCP.
- Ping the default gateway once when present.
- Show the assigned IP address on the completion screen.

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
