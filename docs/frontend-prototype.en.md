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
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali.bin
dist/plumos-frontend/plumos/bin/plumos-safe-hotkeyd
dist/plumos-frontend/plumos/bin/plumos-safe-shutdown
dist/plumos-frontend/plumos/config/frontend/systems.json
dist/plumos-frontend/plumos/config/frontend/menus.json
dist/plumos-frontend/plumos/config/frontend/apps.json
dist/plumos-frontend/plumos/config/frontend/themes.json
dist/plumos-frontend/plumos/config/frontend/settings.json
dist/plumos-frontend/plumos/themes/default/theme.json
dist/plumos-frontend/plumos/lib/
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
The controller UI passes `--with-thumbnails` only when opening a Graphic-mode
ROM list, then uses `media.thumbnail` from `state/frontend/systems/<system>.json`
for the preview panel.

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
- Single canonical thumbnail root: `/mnt/SDCARD/Images/<system_id>`.
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
In Graphic mode, the controller UI adds `--with-thumbnails` so the same cache
also stores thumbnail paths for ROM previews.

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
settings/apps/favorites/network/shutdown. Since the A30 stock MainUI has no
reboot item, `Reboot` is not shown in START menu until a safe reboot path is
known. This prototype only displays actions; it does not execute shutdown.
`menu apps` displays `apps.json` entries where `menu=apps`.

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

`plumos-controller-ui` is the first controller-first prototype. The default
`plumos-controller-ui` renders TOP/ROM-list/START-menu/Favorites/Recent/
Settings state to SSH stdout and reads input from `/dev/input/event*` or stdin
fallback. On the A30 it looks for `gpio-keys-polled` in
`/proc/bus/input/devices`, which normally resolves to `/dev/input/event3`.
`plumos-input-compare` confirms that `/dev/input/event3` can be opened and
polled non-exclusively even while stock `keymon` and stock `MainUI` are running.
The device mapping uses A=`KEY_SPACE`, B=`KEY_LEFTCTRL`, START=`KEY_ENTER`, and
SELECT=`KEY_RIGHTCTRL`. Function=`KEY_ESC` is not an alternate START button; it
opens the SAFE menu.

`plumos-safe-hotkeyd` is the safe-exit helper for the period where the frontend
is blocked by an emulator. It auto-detects `gpio-keys-polled`, reads the
`/dev/input/event3` equivalent non-exclusively, and by default runs
`plumos-safe-shutdown --shutdown --no-poweroff` when Function=`KEY_ESC` is
pressed. `SIGUSR1` uses the same trigger path, so it can be tested without a
physical button press. On 2026-06-08, a NES/RetroArch run triggered by `SIGUSR1`
completed safe shutdown, resume hold, CPU restore, and frontend restart. The
artifact is
`artifacts/a30-probes/safe-shutdown/20260608-165456-safe-hotkeyd-sigusr1-nes`.
`plumos-text-ui launch --execute` automatically starts
`plumos-safe-hotkeyd --oneshot` only while a RetroArch/standalone launch is
running. Set `PLUMOS_SAFE_HOTKEYD_AUTOSTART=0` to disable it. The auto-started
hotkeyd `SIGUSR1` trigger is verified in
`artifacts/a30-probes/safe-shutdown/20260608-170909-text-ui-autohotkey-sigusr1-nes`.
`scripts/probe-a30-safe-hotkeyd.sh` can rerun this path. `--trigger signal`
runs the automated test, while `--trigger physical` waits for a physical
Function press. The latest script-driven signal artifact is
`artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes`.
That artifact also verifies `SAVE_STATE_SLOT 999`, `.state999` creation, and a
pending resume plan using `--entry-slot 999`.
The physical Function press itself is verified in
`artifacts/a30-probes/safe-shutdown/20260608-171641-text-ui-autohotkey-physical-nes`;
`safe-hotkeyd.log` records `trigger source=key`. An in-game SAFE overlay menu
remains to be validated.

`plumos-controller-ui-mali` is the same controller UI with a Mali EGL renderer.
It uses `/dev/fb0` and `/usr/lib/libEGL.so`/`/usr/lib/libGLESv2.so`, without
linking to stock SDL. Its wrapper starts `plumos-controller-ui-mali.bin` through
the bundled dynamic loader/glibc, but does not export `LD_LIBRARY_PATH`. This
prevents scanner calls inside the UI from making the stock `/bin/sh` load the
bundled glibc by accident.

The A30 exposes `/dev/fb0` as a `480x640` portrait framebuffer, while stock
MainUI draws its landscape UI rotated in the raw framebuffer. The Mali renderer
accepts `--rotation auto|none|cw|ccw`; `auto` draws a logical `640x480` UI into
the `480x640` framebuffer in the same raw orientation as stock. A raw capture may
look portrait on a PC, but, like the stock raw capture, it becomes a readable
landscape frame after a 90-degree rotation. That is the expected orientation for
the A30 physical screen.

For plumOS-target tests, stock `MainUI.stock` and `keymon` are expected to be
stopped. Use `--stop-mainui --stop-keymon --no-restart-stock` to pause the stock
`/etc/main` supervisor, stop `MainUI.stock`/`keymon`, and leave the stock side
stopped after the probe exits. Wi-Fi/SSH are maintained by
`wpa_supplicant`/`udhcpc`/`dropbear`, and remained connected after
MainUI/keymon were stopped. Omit `--no-restart-stock` only for comparison runs
where the stock side should be restored.

The boot wrapper does not run `plumos-network-rescue`; it proceeds to the normal
FE TOP screen. The START menu and Network Settings do not expose Network
Recovery. If an old `internal:network-recovery` or `--rescue-network` route is
still invoked, it only shows a disabled compatibility screen and does not call
the helper. Recovery for cases where SSH is unavailable should be designed as a
separate shell script launched directly from StockOS MainUI.

Text sizing, bitmap/FreeType usage, and list column alignment rules for the A30
device UI are documented in [A30 UI Design Rules](a30-ui-design.en.md). In
particular, user-visible text must not be rendered below `1x`, and primary rows
use `2x` as the baseline.

Mali renderer device checks:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --script down,a,b,q
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 2 --exercise 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 30
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock --no-scan --timeout 5 --exercise 2 --rotation auto
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_ROOT=/mnt/SDCARD/plumos /mnt/SDCARD/plumos/bin/plumos-controller-ui-mali --rescue-network --script a,q --timeout 1 --rotation auto'
```

On June 7, 2026, the A30 completed a full scan, displayed TOP, and ran the
`down,a,b,q` script from TOP into a ROM list and back with
`result=frontend_mali_renderer_rc_0`.
The Mali renderer was then changed to an A30-oriented compact layout. Long help
lines are moved into two bottom hint lines, and list rows are space-compacted
with profile details omitted so they fit the 480px width. `--exercise 3`
automatically walked through TOP/ROM/Settings/SAFE, and a 30-second hold while
stock `MainUI.stock` and `keymon` were still running also finished with
`result=frontend_mali_renderer_rc_0`.
`--stop-mainui --stop-keymon --no-restart-stock --rotation auto --exercise 2`
also finished with `result=frontend_mali_renderer_rc_0` in the plumOS target
state with stock `/etc/main`, `MainUI.stock`, and `keymon` stopped. After the
probe, the stock side was left stopped, Wi-Fi/SSH remained connected, and no
stale `plumos-controller-ui-mali` process remained. The `/dev/fb0` capture is
portrait when viewed raw, but, like the stock MainUI capture, it is readable as
landscape after a 90-degree rotation.
At the time, `--rescue-network --script a,q` also succeeded by invoking the
network rescue helper through the Mali UI A action and exiting with code `0`.
As of 2026-06-10, this route is a disabled compatibility screen and no longer
calls the helper.
The Mali renderer draws ASCII with the built-in bitmap font and non-ASCII text
with a FreeType font. Font selection tries `PLUMOS_MALI_FONT`, theme `font_ui`,
then known A30 CJK font candidates. UTF-8 is processed by codepoint instead of
byte, so Japanese ROM names no longer become `???`.
On 2026-06-08, a temporary `PLUMOS_ROOT=/tmp/plumos-fonttest` Japanese ROM list
confirmed `ドラゴンクエストIII`, `悪魔城伝説`, and
`ファイナルファンタジー` in
`artifacts/a30-probes/frontend-font-jp-clean2/20260608-215527.visible.cw.png`.
However, the `reboot` command went dark/LED-off and did not reach the Miyoo logo.
The user held power to force off, powered on again, and at the time restored SSH
by pressing A on the network rescue UI. That FE route is now disabled.

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

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script function,q'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script a,function,up,a,q'
```

Dump raw device button events:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --dump-events --timeout 10'
```

Controls:

- D-pad up/down: move cursor. Physical Up/Down have software key repeat.
- D-pad right/left: page down/up on TOP/ROM list/Favorites/Recent.
- A: enter ROM list on TOP; execute launch on ROM list/Favorites/Recent.
- B: return from ROM list, Favorites, or Recent to TOP. Settings and HELP return
  to START; the System Settings `Display Color` and
  `INFORMATION` subpages return to System Settings; the Network Settings
  `INFORMATION` subpage returns to Network Settings. START returns to the
  previous screen.
- START: open START menu.
- START menu: Settings/Favorites/Recent open real screens; Shutdown runs
  `plumos-safe-shutdown --shutdown --no-poweroff`; other actions show previews.
- Left/Right are never confirm/run/back/cancel. A confirms/runs and B
  backs/cancels.
- SELECT: system/per-ROM core menu.
- Function: open the SAFE menu. SAFE menu contains `Sleep`, `Shutdown`, and
  `Cancel`.
- UI Settings: checkboxes save through A/Left/Right, and choices save through
  Left/Right. TOP/ROM visibility, ordering, ROM scan policy, and boot resume are
  reflected in runtime behavior after saving.
- Network Settings: the first layer contains `Wi-Fi`, `Connect Wi-Fi`,
  `NW Service`, and `INFORMATION`. `Wi-Fi` OFF stops the runtime, connection
  starts through `Connect Wi-Fi`, and `NW Service` owns FTP/SFTP/Samba/USB Disk
  Mode. Connection/IP/Signal details live under `INFORMATION`. SSID/PSK are not
  displayed.
- SSH stdin fallback: `w/s/a/d`, `e` or space, `b`, `m`, `c`, `f`, `q`.

SAFE menu:

- Function opens SAFE menu from any screen.
- Initial cursor is `Cancel` to reduce accidental actions.
- `Sleep` runs `plumos-safe-shutdown --sleep --no-poweroff`.
- `Shutdown` runs `plumos-safe-shutdown --shutdown --no-poweroff`.
- `Cancel` and B return to the previous screen. LEFT/RIGHT and Function are not
  used for confirm/back.
- Power/sleep backend selection is wired in `plumos-safe-shutdown`. The SAFE
  menu default still does not trigger real poweroff or real suspend; it runs
  through save, exit, `sync`, and resume hold.
- While an emulator is running, the direct `plumos-safe-hotkeyd` safe-exit path
  is verified first. The physical Function press is also verified. The in-game
  overlay menu still needs validation.

The Settings screen also shows plumOS-owned system settings, separate from
frontend settings and theme state. UI Settings entries such
as `Show Empty Systems`, `Favorites On TOP`, `Recent On TOP`, `Sort Systems`,
`Sort ROMs`, `Scan On Enter`, and `Boot Resume Mode` are reflected in controller
UI behavior after saving. System Settings reads volume, brightness, lumination,
display color, language, and theme information from
`/mnt/SDCARD/plumos/config/system/settings.json`. Its top level is `Volume`,
`Brightness`, `Lumination`, `Display Color`, `Time Settings`, `Language`,
`Theme`, and `INFORMATION`. `Volume`, `Brightness`, `Lumination`, and
`Language` save to plumOS system settings with Left/Right. `Display Color`
opens a subpage for `Contrast`, `Hue`, and `Saturation`, which also save with
Left/Right. `Time Settings` owns `Timezone` and `Manual Time`; timezone is
applied to the `TZ` environment and runtime `/etc/TZ`, and manual time is
entered as local time in the selected timezone before applying it to the OS
clock. Writes use a first backup, temporary file, fsync, rename, and sync.
Current-value, backend, and policy details live under the `INFORMATION` subpage.
Redacted Wi-Fi runtime status belongs to Network Settings, which does not read
SSID or PSK. CPU mode belongs to Performance Settings. Performance Settings now
connects to the existing `plumos-text-ui core system ... --cpu --freq --cores`
flow and saves per-system `CPU freq` and `CPU Cores` to `core-overrides.json`.
`CPU freq` exposes only fixed `648/816/1200/1344 MHz` values; unpredictable
`keep` is removed. `Reset to Default` falls back to the `systems.json`
`648 MHz` / `2 cores` plumOS defaults.
`Theme` remains read-only
until candidate names and paths are defined safely. In the Mali renderer, the
first Settings row is `HELP`, which opens the
controls help screen. Normal screens do not keep persistent bottom control hints.

Example check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script start,a,q | grep -E "A30|Wi-Fi|Volume|Brightness|Keymap|Input"'
```

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
  7. Shutdown                 system     system:shutdown          yes

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
- `Imgs` contains stock artwork. The normal plumOS thumbnail location is
  `/mnt/SDCARD/Images/<system_id>`; `Imgs` is treated as an importer input if
  migration is needed.
- `RApp/mednafen_wswan/config.json` has `imgpath` set to `../..Imgs/WSC`, which
  appears to be a typo for `../../Imgs/WSC`. Stock compatibility should detect
  and optionally correct cases like this.

## Next Additions

- Match ROM filenames to artwork filenames.
- Parse `gamelist` XML.
- Parse/update `recentlist.json`.
- Build a render test and minimal UI with plumOS-bundled SDL2.
