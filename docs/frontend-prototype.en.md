# Frontend Prototype

The first plumOS frontend is a stock inventory scanner before any real rendering
work. It runs on the A30, reads stock SD-card configuration from `Emu`, `RApp`,
`App`, and `Themes`, and checks the referenced ROM/artwork/metadata paths.

This scanner does not mean stock behavior will become the new plumOS frontend
specification. It gathers evidence for migration, compatibility, or replacement
decisions. The role of `config.json` is documented in
[Stock frontend inventory](stock-frontend-inventory.en.md).

## Build

```sh
./scripts/docker-build.sh frontend
```

Outputs:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/bin/plumos-library-scan
dist/plumos-frontend/plumos/config/frontend/systems.json
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

## Deploy

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

## Manual Run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

Manual mode exits with `0`. Normal mode, as launched by the wrapper, currently
exits with `75` so the wrapper falls back to stock MainUI.
Normal mode suppresses stdout and writes details to
`/mnt/SDCARD/plumos/logs/plumos-frontend.log`. Set `PLUMOS_FRONTEND_STDOUT=1`
to also print to stdout.

## plumOS Library Scan

`plumos-library-scan` is a prototype that reads the plumOS-native
`systems.json`, scans ROMs through Miyoo/ROCKNIX directory aliases, and writes
`library-index.json`.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-library-scan'
```

Default paths:

```text
systems: /mnt/SDCARD/plumos/config/frontend/systems.json
full output:     /mnt/SDCARD/plumos/state/frontend/library-index.json
on-enter output: /mnt/SDCARD/plumos/state/frontend/systems/<system>.json
roms:    /mnt/SDCARD/Roms, /mnt/SDCARD/roms
```

Pass `--on-enter nes` to scan one system and write
`state/frontend/systems/nes.json` without replacing the full cache. The ROM-list
entry refresh flow uses this behavior.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-library-scan --on-enter nes'
```

`--on-enter` prioritizes the first text-mode display and defers thumbnail lookup
by default. Use `--with-thumbnails` when thumbnails should be resolved during the
scan.

Environment:

- `PLUMOS_SDCARD_ROOT`: SD card root. Default: `/mnt/SDCARD`
- `PLUMOS_ROOT`: plumOS root. Default: `$PLUMOS_SDCARD_ROOT/plumos`
- `PLUMOS_SYSTEMS_JSON`: system definition file
- `PLUMOS_LIBRARY_INDEX`: generated cache file

Implemented:

- Initial `systems.json` seed.
- Miyoo uppercase aliases and ROCKNIX lowercase alias scanning.
- Recursive ROM scan including subdirectories.
- ROM extension filtering.
- `RomEntry` generation.
- Thumbnail lookup that preserves paths relative to the ROM alias root.
- Subdirectory artwork priority, flat artwork fallback, and placeholder
  fallback.
- Case-insensitive lookup for `png`, `jpg`, `jpeg`, and `webp`.
- Duplicate scan suppression for case variants such as `Roms`/`roms` and
  `GBA`/`gba`.
- Per-system cache output for `--on-enter`.
- Deferred thumbnail lookup for the first text-mode display.

## ROM Scan Benchmark

Run the 1000 dummy ROM benchmark with:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/benchmark-a30-rom-scan.sh 1000
```

Dummy files are created only under `/mnt/SDCARD/plumos/tmp/rom-scan-bench` and
removed after the run.

A30 device result on 2026-06-06:

```text
system nes roms=1000 thumbnails=0
timing load_ms=10 scan_ms=362 sort_ms=2 ready_ms=374 write_ms=29 total_ms=403
summary alias_dirs=1 files_seen=1000 matched=1000 roms=1000 thumbnails=0 elapsed_ms=403
```

`ready_ms=374` is below the 500 ms threshold for the first text-mode display, so
plumOS keeps the on-enter scan policy instead of switching to stock-style manual
refresh.

## Current Inputs

- `/mnt/SDCARD/Emu/*/config.json`
- `/mnt/SDCARD/RApp/*/config.json`
- `/mnt/SDCARD/App/*/config.json`
- `/mnt/SDCARD/Themes/*/config.json`
- Existence, regular-file count, and `extlist` match count for `rompath`
  directories.
- Existence and image-file count for `imgpath` artwork directories.
- Existence and size for `gamelist` files.
- `launchlist` entry count.
- Existence, size, and non-empty-line entry count for
  `/mnt/SDCARD/Roms/recentlist.json`

Fields currently read:

- `label`
- `name`
- `launch`
- `rompath`
- `imgpath`
- `extlist`
- `gamelist`
- `launchlist`

`rompath`, `imgpath`, and `gamelist` are resolved relative to the directory that
contains each `config.json`. `extlist` matching is case-insensitive. The scanner
does not log ROM/artwork filenames yet; it only records counts.

## Device Check

Manual mode was run on the A30 on 2026-06-06 and confirmed:

```text
summary configs emu=18 rapp=27 app=5 themes=42
summary roms dirs=25 files=0 matched=0
summary artwork dirs=41 images=4027
summary metadata gamelists=0 launchers=22
```

`Roms/recentlist.json` was detected as `size=234 entries=3`. The scanner does
not log the values; it only treats non-empty lines as entries for now.

Observations:

- On the current SD card, many configured ROM directories exist, but ROM files
  are almost empty.
- `Imgs` contains existing artwork, so plumOS should be able to reference
  artwork without moving it.
- `RApp/mednafen_wswan/config.json` has `imgpath` set to `../..Imgs/WSC`, which
  appears to be a typo for `../../Imgs/WSC`. Stock compatibility should detect
  and optionally correct cases like this.

## Next Additions

- Match ROM filenames to artwork filenames.
- Parse `gamelist` XML.
- Parse/update `recentlist.json`.
- Build the first SDL/input UI.
