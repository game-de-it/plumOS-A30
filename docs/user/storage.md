# SD Card Layout

plumOS release archives are extracted directly to the SD-card root. This page
separates user-managed directories from runtime directories.

## Top-Level Directories

| Path | Purpose |
| --- | --- |
| `Roms/` | ROMs and game data |
| `Bios/` | BIOS and system files |
| `Images/` | Normal plumOS thumbnail directory |
| `Imgs/` | Legacy StockOS-compatible artwork directory |
| `Saves/` | Save/state compatibility directory |
| `plumos/` | plumOS runtime, configuration, logs |
| `miyoo/` | StockOS SD payload and plumOS boot wrapper |
| `Emu/` | StockOS emulator payload |
| `RetroArch/` | StockOS RetroArch payload |
| `RApp/` | StockOS/compatibility app payload |
| `App/` | StockOS/compatibility app payload |
| `Themes/` | StockOS theme payload |

## ROM Directories

ROMs go under `Roms/<directory>/`. plumOS recognizes both Miyoo-style uppercase
short names and EmulationStation-style lowercase names.

Examples:

```text
Roms/FC/
Roms/nes/
Roms/SFC/
Roms/snes/
Roms/GBA/
Roms/gba/
```

See [Supported Systems and Emulator Profiles](emulators.md) for directory names
and extensions.

## Thumbnail Directories

The normal thumbnail path is `Images/<ROM directory name>/`. Match the directory
name used under `Roms/`.

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png
```

If the ROM is under `Roms/SFC/`, use `Images/SFC/`.

`Imgs/` remains only for StockOS compatibility and legacy artwork.

## BIOS Directories

BIOS files go under `Bios/`.

```text
Bios/
Bios/psx/
Bios/pcenginecd/
```

Some cores look at `Bios/<ROM directory name>/`; others look at `Bios/` itself.
Required BIOS names and placement depend on the emulator/core.

## `plumos/`

Most users do not need to edit these files directly, but they are important for
backup and troubleshooting.

| Path | Purpose |
| --- | --- |
| `plumos/bin/` | Launchers, helpers, frontend wrappers |
| `plumos/config/frontend/` | Frontend settings and system definitions |
| `plumos/config/system/` | Volume, brightness, language, device settings |
| `plumos/config/network/` | Network service settings |
| `plumos/config/performance/` | Performance settings |
| `plumos/config/standalone/` | Launcher environment overrides |
| `plumos/logs/` | Runtime logs |
| `plumos/retroarch/` | plumOS RetroArch runtime |
| `plumos/retroarch/cores/` | plumOS libretro cores |
| `plumos/state/` | Runtime state |
| `plumos/factory-defaults/` | Factory reset defaults |
| `plumos/ssh/` | Dropbear SSH/SFTP runtime |
| `plumos/themes/` | plumOS frontend themes |
| `plumos/python/` | Python runtime |

## What to Back Up

Back up these paths before recreating an SD card:

- `Roms/`
- `Bios/`
- `Images/`
- `Imgs/`
- `Saves/`
- `plumos/config/`
- `plumos/state/`
- `plumos/retroarch/saves/`
- `plumos/retroarch/states/`

## Japanese Counterpart

- [Japanese storage guide](storage.ja.md)
