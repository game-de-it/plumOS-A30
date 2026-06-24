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
- `deploy-a30.sh` must not overwrite existing mutable settings with package
  defaults. It installs defaults on first deploy and restores existing files
  after extraction when they already exist. Protected files include
  `config/*/settings.json` and the main RetroArch configs
  `retroarch/config/retroarch-minimal.cfg` /
  `retroarch/config/retroarch-practical.cfg`, plus standalone emulator env
  overrides in `config/standalone/` and PPSSPP runtime settings under
  `state/standalone/ppsspp/`.

## Current Layout

- `/mnt/SDCARD/plumos/config/frontend/settings.json`
  - Frontend behavior such as UI Mode, Graphic Theme, TOP/ROM list visibility,
    sorting, scan, and Open Last ROM At Boot.
- `/mnt/SDCARD/plumos/config/frontend/menus.json`
  - Frontend menu definitions, including the START menu.
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - System definitions, ROM directory aliases, launch profiles, and artwork
    lookup.
- `/mnt/SDCARD/plumos/config/system/settings.json`
  - plumOS-owned device settings: Volume, Brightness, Lumination, Display Color,
    Time Settings, and Language.
- `/mnt/SDCARD/plumos/share/frontend/lang/*.lang`
  - Frontend UI string dictionaries. The `language` setting selects one of
    these `key=value` files for item labels and help text. Missing keys fall
    back to English strings compiled into the FE.
- `/mnt/SDCARD/plumos/factory-defaults/{ra,pico,sa}/`
  - Files saved as factory defaults for RA, PicoArch, and standalone emulator
    settings.
  - Paths are preserved relative to `/mnt/SDCARD/plumos/`. Example:
    `factory-defaults/ra/retroarch/home/.config/retroarch/retroarch.cfg`.
  - `plumos-factory-reset` only restores files present here. ROMs, BIOS files,
    save data, shader caches, and similar user data are out of scope.

## Future Candidates

- `/mnt/SDCARD/plumos/config/network/settings.json`
  - Wi-Fi enable policy and network recovery policy. SSID/PSK should be managed
    separately.
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - Existing system/ROM launch profile and CPU policy/frequency/core overrides.
  - Performance Settings now connects to this existing feature.
- `/mnt/SDCARD/plumos/config/performance/profiles.json`
  - Future global presets or named performance profiles, if needed.
- `/mnt/SDCARD/plumos/config/standalone/<emulator>.env`
  - Environment overrides for standalone launchers. PPSSPP display, input, CPU,
    and similar device-tuned launch conditions belong in `ppsspp.env`, not in
    the generated `plumos-standalone-launch` script.
  - PicoArch launch defaults belong in `picoarch.env`. Its BIOS/system
    directory can be set with `PLUMOS_PICOARCH_BIOS_DIR=/path`, but a
    per core/ROM directory PicoArch `picoarch.cfg` entry `bios_dir = /path`
    takes precedence.
  - PCSX-ReARMed standalone reads `PLUMOS_A30_PSX_BIOS_DIR`, which defaults to
    `/mnt/SDCARD/Bios`, because upstream standalone PCSX normally scans
    `$HOME/.pcsx/bios`.
  - This directory is protected by `deploy-a30.sh`, so standalone package
    redeploys keep local tuning.
  - PPSSPP `ppsspp.ini` / `controls.ini` are user-managed PPSSPP settings and
    are not repaired during normal launcher startup. Enable
    `PLUMOS_A30_PSP_CONTROLS=standard` only when controls repair is explicitly
    needed.
- `/mnt/SDCARD/plumos/config/input/mapping.json`
  - plumOS input mapping, hotkeys, and physical button integration.

## System Settings

System Settings reads and writes
`/mnt/SDCARD/plumos/config/system/settings.json`. It is separate from stockOS
`/config/system.json`.

Current keys:

- `volume`: `0..20`
- `brightness`: `1..20`
- `lumination`: `0..10`
- `contrast`: `0..20`
- `hue`: `0..20`
- `saturation`: `0..20`
- `language`: `en.lang`, `ja.lang`, etc. Matching dictionaries are loaded from
  `/mnt/SDCARD/plumos/share/frontend/lang/`.
- `timezone`: POSIX TZ string. Default is `JST-9`

As of 2026-06-20, `Language` selects the frontend `.lang` dictionary and is
applied to Settings item labels and help text. Values other than `Language` are
applied to the A30 runtime when saved and once during FE startup. `volume` is applied through
`plumos-volume-control` to the ALSA default PCM `Soft Volume Master`, and
RetroArch/standalone emulators use ALSA `default` so they share the same volume
setting. `brightness` uses `/sys/devices/virtual/disp/disp/attr/lcdbl`, and
`lumination` / `contrast` / `hue` / `saturation` use
`/sys/devices/virtual/disp/disp/attr/enhance`.
`Soft Volume Master` is created only after the ALSA default PCM is opened once,
so the FE and launchers initialize it with short silence playback when needed.
Brightness stores `1..20`; RAW lcdbl is mapped to
`2,3,4,5,6,7,8,9,10,26,43,59,75,92,108,125,141,157,174,190`.
`timezone` is sourced from plumOS config and applied to the `TZ` environment
and runtime `/etc/TZ` when saved, at FE startup, and from the MainUI wrapper.
plumOS still does not write stockOS `/config/system.json`. Manual time entry is
interpreted as local time in the selected timezone, converted to UTC epoch, and
then applied to the OS clock.

### Factory Reset

System Settings `Factory Reset` calls `plumos-factory-reset` and restores only
emulator settings saved under `factory-defaults/{ra,pico,sa}/`. Each reset
action requires pressing A twice to avoid accidental resets.

Before restore, existing files are copied to
`/mnt/SDCARD/plumos/backups/factory-reset/<timestamp>/<target>/`. Restore is a
file-level overwrite within the selected target; save data, BIOS files, ROMs,
thumbnails, and logs are not deleted.

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
