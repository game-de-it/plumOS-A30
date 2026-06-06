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
dist/plumos-frontend/plumos/bin/plumos-text-ui
dist/plumos-frontend/plumos/bin/plumos-controller-ui
dist/plumos-frontend/plumos/config/frontend/systems.json
dist/plumos-frontend/plumos/config/frontend/menus.json
dist/plumos-frontend/plumos/config/frontend/apps.json
dist/plumos-frontend/plumos/config/frontend/settings.json
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

## plumOS Text UI

`plumos-text-ui` is an SSH-facing text UI prototype for validating the system
list and ROM list data flow before real rendering/input work. It is not
auto-launched as the MainUI replacement yet.

TOP view:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui top'
```

`top` reads the existing `library-index.json`. If the cache is missing, it runs
a full scan. Use `top --refresh` to refresh explicitly.
When `show_favorites_on_top=true` in `settings.json`, `Favorites` appears as a
virtual TOP system. The virtual entry opens `internal:favorites`.

ROM list view:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui roms ports --limit 10'
```

`roms <system>` runs `plumos-library-scan --on-enter <system>` internally, then
reads `state/frontend/systems/<system>.json`. This is the first prototype of the
future "re-read ROM list whenever entering a system" behavior.

START menu view:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui menu start'
```

Apps submenu view:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui menu apps'
```

`menu start` reads `menus.json` and displays the path to
settings/apps/favorites/network/reboot/shutdown. `Reboot` and `Shutdown` are
defined with `confirm=yes`. This prototype only displays actions; it does not
execute reboot/shutdown. `menu apps` displays `apps.json` entries where
`menu=apps`.

System core selection view:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core system nes'
```

Set a system default core/profile:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core system nes --set retroarch:nestopia'
```

Set a per-ROM core/profile override:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core rom nes FC/example.nes --set retroarch:fceumm'
```

`core system` corresponds to pressing SELECT on a highlighted TOP system.
`core rom` corresponds to pressing SELECT on a highlighted ROM list entry. State
is saved to `state/frontend/core-overrides.json`. The stored value is a
`launch_profile` id, not a core `.so` path. Priority is
`ROM override > system override > default_launch_profile > auto detect`.
`--clear` removes the matching override and falls back to the next layer.

Favorite toggle:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui favorite rom ports "PORTS/Start SSH.sh" --toggle'
```

Favorites list:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui favorites'
```

Open Favorites through the TOP virtual system flow:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui roms favorites'
```

`favorite rom` corresponds to pressing the favorite toggle in the ROM list.
State is saved to `state/frontend/favorites.json`. The ROM list `Fav` column
shows `*` for favorite entries. The START menu `Favorites` entry is the path to
this Favorites list. When `show_favorites_on_top=true`, Favorites can also be
opened from the TOP screen.

Add and list Recent:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui recent add ports "PORTS/Start SSH.sh" --profile external:port'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui recent'
```

Set and show Resume session:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui resume set ports "PORTS/Start SSH.sh" --profile external:port --reason shutdown'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui resume show'
```

Boot resume decision:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui boot'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui boot --execute'
```

`recent.json` is the browsing history; `resume-session.json` is the next-boot
resume candidate. `boot_resume_mode` in `settings.json` accepts `off`, `last`,
and `picker`. By default, `boot` only prints the decision and launch plan.
With `--execute`, it launches the same ROM/launch profile only when
`boot_resume_mode=last` and a pending resume session exists. RetroArch profiles
are executable only when `/mnt/SDCARD/plumos/retroarch/bin/retroarch` and
`/mnt/SDCARD/plumos/retroarch/cores/<core>_libretro.so` exist. Real Auto State
Load integration belongs to the later RetroArch/launcher implementation.

When `plumos-frontend` starts in boot mode, it calls
`/mnt/SDCARD/plumos/bin/plumos-text-ui boot --execute` once. The default
`boot_resume_mode=off` only continues to the normal TOP flow.

## plumOS Controller UI

`plumos-controller-ui` is the first controller-first prototype. It does not draw
to framebuffer/SDL yet. Instead, it renders TOP/ROM-list/START-menu/Favorites/
Recent/Settings state to SSH stdout and reads input from `/dev/input/event*` or
stdin fallback. On the A30 it looks for `gpio-keys-polled` in
`/proc/bus/input/devices`, which normally resolves to `/dev/input/event3`.

Render TOP once:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --once --no-clear'
```

Check state transitions with scripted input:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script down,a,b,select,start,q'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script start,a,b,start,down,down,a,b,start,down,down,down,a,b,q'
```

Dump raw device button events:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --dump-events --timeout 10'
```

Controls:

- D-pad: move cursor.
- A/right: enter ROM list on TOP; show launch preview on ROM list.
- B/left: return from ROM list, Favorites, Recent, or Settings to TOP. In START
  menu, return to the previous screen.
- START: open START menu.
- START menu: Settings/Favorites/Recent open real screens; other actions show
  previews.
- SELECT: system/per-ROM core preview.
- Settings: show current values; A shows edit preview.
- SSH stdin fallback: `w/s/a/d`, `e` or space, `b`, `m`, `c`, `q`.

A30 device check on 2026-06-06:

```text
plumOS text UI - TOP
No.  System                 ROMs  Default profile
  1. Ports                     2  external:port

plumOS text UI - ROM list
system: ports
ready_ms: 10
  1.     Start SSH                          PORTS/Start SSH.sh
  2.     Stop SSH                           PORTS/Stop SSH.sh

plumOS text UI - menu
menu: start
  1. Settings                 internal   internal:settings        no
  2. Apps                     submenu    menu:apps                no
  3. Favorites                internal   internal:favorites       no
  4. Recent                   internal   internal:recent          no
  5. Network                  internal   internal:network         no
  6. Refresh Current System   scan       scan:current             no
  7. Reboot                   system     system:reboot            yes
  8. Shutdown                 system     system:shutdown          yes

plumOS text UI - core selection
scope: system
system: nes (NES)
current: retroarch:fceumm (plumOS default)
  1. retroarch:fceumm               yes      no       -        *
  2. retroarch:nestopia             no       no       -

plumOS text UI - Favorites
count: 0
```

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
