# Stock Frontend Inventory

This document is not a specification for copying the A30 stock frontend into
plumOS. It is an investigation note for understanding the existing SD-card
layout and deciding whether migration or compatibility shims are needed.

The plumOS frontend/launcher specification will be designed separately. If a
stock behavior should be reused directly, document the reason, alternatives, and
impact first, then ask the user for confirmation before implementing it.

## Role Of `config.json`

The stock A30 MainUI appears to read multiple `config.json` files from the SD
card to decide menu entries, ROM lists, artwork lookup, and launch scripts.

Main locations:

- `/mnt/SDCARD/Emu/*/config.json`: primary emulator categories.
- `/mnt/SDCARD/RApp/*/config.json`: additional or alternate libretro cores.
- `/mnt/SDCARD/App/*/config.json`: apps/tools.
- `/mnt/SDCARD/Themes/*/config.json`: themes.

Representative example:

```json
{
  "label": "GBA",
  "icon": "gba.png",
  "launch": "launch.sh",
  "rompath": "../../Roms/GBA",
  "imgpath": "../../Imgs/GBA",
  "useswap": 0,
  "shortname": 0,
  "hidebios": 0,
  "extlist": "gba|agb|gbz",
  "launchlist": [
    { "name": "GPSP", "launch": "launch.sh" },
    { "name": "MGBA", "launch": "launch1.sh" }
  ]
}
```

## How Stock MainUI Uses It

Current understanding:

- `label`: display name.
- `icon`, `iconsel`: menu icons.
- `rompath`: ROM directory.
- `imgpath`: artwork directory.
- `extlist`: extension filter for the ROM list.
- `launch`: shell script called after selecting a ROM.
- `launchlist`: alternate launcher/core candidates.
- `gamelist`: metadata file for arcade-like systems.
- `useswap`, `shortname`, `hidebios`: likely stock MainUI display/scan options.

The important point is that `config.json` does not fully define runtime
behavior. RetroArch startup, CPU governor, CPU online/offline state, overclock,
`HOME`, and `EMU_DIR` are spread across per-system `launch.sh` scripts.

Example:

```sh
echo 0 > /sys/devices/system/cpu/cpu3/online
echo 0 > /sys/devices/system/cpu/cpu2/online
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
$EMU_DIR/overclock 816
HOME=/mnt/SDCARD/RetroArch/ retroarch -v -L core.so "$1"
```

Some `RApp/*/launch.sh` files use `EMU_DIR` without defining it. This suggests,
but does not prove yet, that stock MainUI sets `EMU_DIR` before launching, or
that another wrapper path is involved.

## Treatment In plumOS

Do not adopt by default:

- Do not make stock `config.json` the official plumOS frontend spec.
- Do not make stock `launch.sh` files the new launcher spec.
- Do not distribute CPU policy across shell scripts.
- Do not make `HOME=/mnt/SDCARD/RetroArch` a requirement for the new RetroArch
  runtime.

Acceptable uses:

- Input for the existing-SD-card inventory scanner.
- Compatibility input for migrating existing ROMs/artwork/saves/states.
- Temporary compatibility shims for early rollback and comparison testing.

Design separately for plumOS:

- Frontend system/app/theme data model.
- ROM/artwork/recent/favorite management.
- RetroArch/core launch profiles.
- CPU/governor/clock policy.
- Save/state/config layout.

## Decision Gate

Before reusing a stock behavior directly, check:

- What exactly is being reused?
- Why is it better than a plumOS-native design?
- Will it block future RetroArch/core updates?
- Is a compatibility shim enough for existing-SD-card support?
- Has the user explicitly approved this reuse?

This rule applies to `config.json`, `launch.sh`, CPU policy, RetroArch startup,
theme layout, and recent/favorite formats.

Use the [Stock reuse decision template](decisions/stock-reuse-template.md)
for decision records.
