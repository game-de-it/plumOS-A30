# Save Data and States

plumOS does not force every emulator into one global save directory. RetroArch,
PicoArch, standalone emulators, and Pyxel use different runtime homes and
configuration files. Back up the paths below before recreating an SD card.

An in-game save and a save state are different things:

- In-game saves are SRAM, memory cards, battery saves, RPG save files, and
  similar data created by the game or emulator core.
- Save states are emulator snapshots. They usually work only with the same
  emulator/core family and can break across major emulator changes.

## Quick Backup Checklist

For a broad user-data backup, copy these paths from the SD card:

- `Roms/`
- `Bios/`
- `Images/`
- `Imgs/`
- `Saves/` if you imported or manually placed data there
- `plumos/config/`
- `plumos/state/`
- `plumos/retroarch/home/.config/retroarch/`

`Saves/` is kept as a compatibility and manual-use directory. It is not the
single default save location for all plumOS emulator runtimes.

## RetroArch Profiles

Frontend RetroArch launches use:

- Launcher: `plumos/bin/plumos-retroarch-launch`
- Runtime wrapper: `plumos/bin/plumos-retroarch-practical`
- Main config: `plumos/retroarch/config/retroarch-practical.cfg`
- RetroArch home: `plumos/retroarch/home/`

The practical RetroArch config currently sets:

| Setting | Value |
| --- | --- |
| `savefile_directory` | `~/.config/retroarch/saves` |
| `savestate_directory` | `~/.config/retroarch/states` |
| `savefiles_in_content_dir` | `true` |
| `savestates_in_content_dir` | `true` |
| `savestate_auto_save` | `true` |
| `savestate_auto_load` | `false` |
| `savestate_auto_index` | `true` |
| `screenshot_directory` | `/mnt/SDCARD/Images` |

Because `savefiles_in_content_dir` and `savestates_in_content_dir` are enabled,
normal RetroArch save files and states are expected to be written beside or
under the ROM content directory. The exact file name and subdirectory can differ
by core and RetroArch setting.

The fallback base directories are:

```text
plumos/retroarch/home/.config/retroarch/saves/
plumos/retroarch/home/.config/retroarch/states/
```

Screenshots are saved under `Images/`. They are artwork/user media, not game
save data.

If you change RetroArch directory settings from the RetroArch menu, follow the
paths shown in RetroArch itself and include those paths in your backup.

## PicoArch Profiles

Frontend PicoArch launches use:

- Launcher: `plumos/bin/plumos-picoarch-launch`
- Runtime state root: `plumos/state/picoarch/`

The launcher sets PicoArch's environment like this:

```text
HOME=plumos/state/picoarch
XDG_CONFIG_HOME=plumos/state/picoarch/xdg-config
XDG_DATA_HOME=plumos/state/picoarch/xdg-data
XDG_CACHE_HOME=plumos/state/picoarch/xdg-cache
```

Per-core and per-ROM PicoArch configuration is created under directories such
as:

```text
plumos/state/picoarch/.picoarch-<core>-<rom-tag>/picoarch.cfg
```

PicoArch save data, save states, core options, and cache files should normally
be treated as part of `plumos/state/picoarch/`. Some libretro cores can still
write content-specific files beside the ROM, so keep `Roms/` in your backup if
you rely on PicoArch.

PicoArch BIOS lookup is configured separately. The default plumOS path is
`Bios/`, and the launcher can write `bios_dir` into per-core PicoArch configs.

## Standalone Profiles

Standalone emulator launches use:

- Launcher: `plumos/bin/plumos-standalone-launch`
- Runtime state root: `plumos/state/standalone/<emulator-id>/`

For each standalone emulator, plumOS sets:

```text
HOME=plumos/state/standalone/<emulator-id>
XDG_CONFIG_HOME=plumos/state/standalone/<emulator-id>/config
XDG_DATA_HOME=plumos/state/standalone/<emulator-id>/data
```

Common standalone locations:

| Runtime | Main save/config area |
| --- | --- |
| PPSSPP | `plumos/state/standalone/ppsspp/`; config is under `config/ppsspp/PSP/SYSTEM/`. PPSSPP save data and states live in this PPSSPP home tree. |
| PCSX-ReARMed standalone | `plumos/state/standalone/pcsx_rearmed/` |
| ScummVM standalone | `plumos/state/standalone/scummvm/`; plumOS passes `config/scummvm.ini` unless overridden |
| EasyRPG standalone | `plumos/state/standalone/easyrpg/`; also back up the game project under `Roms/` because RPG Maker saves may be project-relative |
| OpenBOR standalone | `plumos/state/standalone/openbor/Saves/`; logs and screenshots are also under `plumos/state/standalone/openbor/` |

If a standalone emulator has its own in-app save path setting, that setting can
override the plumOS default. In that case, back up the path shown by the
emulator.

## Pyxel

Pyxel launches through the plumOS Pyxel wrapper. By default, the wrapper sets:

```text
HOME=/mnt/SDCARD
```

That means Pyxel apps can create user data under the SD-card root unless the app
chooses a more specific path. If `PLUMOS_PYXEL_HOME` is changed, use that path
instead. Back up the Pyxel app directory and any data files created by the app.

## Factory Reset

The emulator factory reset restores plumOS-provided emulator configuration
defaults. It is not intended to delete ROMs, BIOS files, screenshots, or user
game saves. Because some emulator configs live next to runtime state, make a
backup before using factory reset if you have manually changed emulator paths or
controls.

## Japanese Counterpart

- [Japanese save-data guide](save-data.ja.md)
