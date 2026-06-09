# plumOS Config Layout Policy

All persistent plumOS-owned files live under `/mnt/SDCARD/plumos/`. plumOS does
not treat stockOS `/config` files or stock frontend settings as normal writable
plumOS configuration.

## Basic Policy

- Do not collapse every setting into one large `system.json`.
- Split files by responsibility, such as UI, system, network, and performance.
- Use human-readable JSON for settings.
- Save with a temporary file, `fsync`, atomic rename, and `sync`.
- Put `version` in each file for compatibility.
- Do not store sensitive data in normal config files or git-tracked files.
- Do not use stockOS-owned `/config/system.json` as a read/write target.

## Current Layout

- `/mnt/SDCARD/plumos/config/frontend/settings.json`
  - Frontend behavior such as UI Mode, TOP/ROM list visibility, sorting, scan,
    and boot resume.
- `/mnt/SDCARD/plumos/config/frontend/menus.json`
  - Frontend menu definitions, including the START menu.
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - System definitions, ROM directory aliases, launch profiles, and artwork
    lookup.
- `/mnt/SDCARD/plumos/config/system/settings.json`
  - plumOS-owned device settings: Volume, Brightness, Lumination, Display Color,
    Language, and Theme.

## Future Candidates

- `/mnt/SDCARD/plumos/config/network/settings.json`
  - Wi-Fi enable policy and network recovery policy. SSID/PSK should be managed
    separately.
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - Existing system/ROM launch profile and CPU policy/frequency/core overrides.
  - Performance Settings now connects to this existing feature.
- `/mnt/SDCARD/plumos/config/performance/profiles.json`
  - Future global presets or named performance profiles, if needed.
- `/mnt/SDCARD/plumos/config/input/mapping.json`
  - plumOS input mapping, hotkeys, and physical button integration.

## System Settings

System Settings reads and writes
`/mnt/SDCARD/plumos/config/system/settings.json`. It is separate from stockOS
`/config/system.json`.

Current keys:

- `volume`: `0..20`
- `brightness`: `0..10`
- `lumination`: `0..10`
- `contrast`: `0..20`
- `hue`: `0..20`
- `saturation`: `0..20`
- `language`: `en.lang`, `ja.lang`, etc.
- `theme`: plumOS theme id

## Performance Settings

Performance Settings should not invent a new cpufreq model first. It should
connect to the existing `plumos-text-ui core system|rom ... --cpu --freq --cores`
flow and `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`.
`systems.json` provides `default_cpu_policy` / `default_cpu_freq_khz`; system
overrides and then ROM overrides take precedence. The plumOS default is fixed
to `648 MHz` / `2 cores` for every system.

As of 2026-06-09, START `Performance Settings` lets the user choose a system
and change `CPU freq` and `CPU Cores` with Left/Right. `CPU freq` exposes only
fixed `648/816/1200/1344 MHz` values; unpredictable `keep` is removed from the
user-facing UI. The controller UI saves by calling `plumos-text-ui core system
<system> --cpu ... --freq ... --cores ...`, which writes `core-overrides.json`.
`Reset to Default` calls `--clear-cpu` and falls back to the `systems.json`
`648 MHz` / `2 cores` plumOS defaults.
