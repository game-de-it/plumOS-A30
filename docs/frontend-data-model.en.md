# plumOS Frontend Data Model

This document is the first plumOS-native frontend data model design. The stock
MainUI `config.json` files are useful reference input, but they are not the
official plumOS specification.

## Goals

- The top screen shows supported systems.
- System labels are short, for example `NES`, `GB`, and `Mega Drive`.
- Avoid manufacturer-heavy labels such as `Nintendo - FC` or `SEGA - MD`.
- Recognize both Miyoo-style uppercase abbreviated ROM folders and
  ROCKNIX/EmulationStation-style lowercase ROM folders.
- Text mode must operate TOP/ROM/settings screens without icons or images.
- Gallery mode uses images. The ROM list shows one ROM plus one thumbnail per
  screen and slides left/right.
- Re-scan the ROM directory when entering a system so newly copied ROMs appear
  without a manual refresh.
- Settings, network, power, tools, and non-emulator apps are reached through a
  START menu.
- Existing ROMs/artwork should be readable without moving them.
- RetroArch/core startup, CPU policy, and save/state layout are handled by
  separate profiles.

## Principles

- A game platform is a `SystemDefinition`.
- ROM folder names are `DirectoryAlias` values, not system identities.
- One system can have multiple directory aliases.
- One directory alias can be shared by multiple systems.
- Shared aliases are routed by extension, e.g. Miyoo `GB` can feed both `GB`
  and `GBC`.
- The top-screen order is controlled by `SystemDefinition.sort_order`.
- By default, show only systems that contain ROMs.
- `pinned=true` can force a system to appear even when it is empty.
- UI mode and library data are separate. Text mode must work without images.

## Runtime Directory

```text
/mnt/SDCARD/plumos/
  config/frontend/
    systems.json
    apps.json
    menus.json
    themes.json
    settings.json
  state/frontend/
    library-index.json
    recent.json
    favorites.json
    play-history.json
    resume-session.json
    core-overrides.json
    cursor-state.json
    scan-stats.json
  cache/frontend/
    render-cache/
    text-index/
  frontend/
    themes/
    icons/
    fonts/
```

`systems.json` is the human-maintained source of truth. `library-index.json` is
generated scan output.

## Core Entities

### `SystemDefinition`

The unit shown on the TOP screen.

```json
{
  "id": "megadrive",
  "display_name": "Mega Drive",
  "short_name": "MD",
  "sort_order": 120,
  "hardware": "console",
  "enabled": true,
  "pinned": false,
  "directory_aliases": [
    { "name": "MD", "source": "miyoo", "priority": 10 },
    { "name": "megadrive", "source": "rocknix", "priority": 20 },
    { "name": "genesis", "source": "rocknix", "priority": 30 }
  ],
  "scan_directories": false,
  "extensions": ["gen", "md", "smd", "32x", "bin", "chd", "zip", "7z"],
  "artwork": {
    "lookup": [
      { "root": "sdcard", "path": "Images/megadrive" }
    ]
  },
  "launch_profiles": ["retroarch:genesis_plus_gx", "retroarch:picodrive"],
  "default_launch_profile": "retroarch:genesis_plus_gx"
}
```

Important fields:

- `id`: lowercase ASCII plumOS key used by state/history/launch profiles.
- `display_name`: short UI label.
- `short_name`: extra-short label for tight spaces.
- `directory_aliases`: candidate ROM directory names.
- `scan_directories`: when true, first-level directories under the alias root are
  treated as ROM entries. This is for directory-based systems such as ScummVM and
  EasyRPG.
- `extensions`: ROM extensions without dots.
- `artwork.lookup`: thumbnail/cover lookup path. Normal systems use only
  `/mnt/SDCARD/Images/<system_id>`.
- `scraper`: optional per-system policy for thumbnail scraper eligibility and
  CRC/download worker counts. When omitted, treat the system as scraper-excluded
  or use the global default.
- `launch_profiles`: launcher-side candidates; the frontend does not execute
  cores directly.

### Launch Profile

`launch_profiles` and `default_launch_profile` use `kind:id` strings. The FE
currently handles:

- `retroarch:<core_id>`: launches
  `/mnt/SDCARD/plumos/retroarch/cores/<core_id>_libretro.so` through the
  RetroArch launcher.
- `picoarch:<core_id>`: launches the same libretro core through the lightweight
  PicoArch frontend.
- `standalone:<emulator_id>`: launches a standalone runtime under
  `/mnt/SDCARD/plumos/emulators/<emulator_id>/`.
- `pyxel:a30`: launches `.pyxapp` / `.py` content through the Pyxel launcher.

`picoarch:<core_id>` shares libretro cores with RetroArch, but its runtime path
is `/mnt/SDCARD/plumos/bin/plumos-picoarch-launch`. CPU policy and core count
settings are passed as `PLUMOS_PICOARCH_*` environment variables.
In Core Settings, the FE automatically adds a matching `picoarch:<core_id>`
candidate for each `retroarch:<core_id>` listed in `systems.json`. The initial
default remains `default_launch_profile`; users opt into PicoArch explicitly.
Cores that fail real A30 display, input, or full-speed validation are excluded
from candidates. As of 2026-06-16, `tgbdual` is removed from the normal GB/GBC
candidate list and also blocked from PicoArch automatic companions.
For PC Engine-family systems, `mednafen_pce` is excluded from normal RA/PICO
candidates because of its runtime load; use `mednafen_pce_fast` instead.

### `DirectoryAlias`

```json
{
  "name": "FC",
  "source": "miyoo",
  "priority": 10,
  "shared": true
}
```

Miyoo aliases are uppercase abbreviations such as `FC`, `MD`, and `WS`.
ROCKNIX/EmulationStation aliases are lowercase names such as `nes`,
`megadrive`, and `wonderswan`.

Examples of shared aliases:

- `GB` can feed `gb` and `gbc`, routed by `.gb` and `.gbc`.
- `FC` can feed `nes` and `fds`, routed by `.nes` and `.fds`.
- `MD` may feed `megadrive`, `mastersystem`, `gamegear`, and `sega32x`,
  depending on extension.

### `RomEntry`

Generated scan result. It is not hand-written.

```json
{
  "id": "sha1:...",
  "system_id": "gba",
  "path": "/mnt/SDCARD/Roms/GBA/example.gba",
  "relative_path": "GBA/example.gba",
  "file_name": "example.gba",
  "title": "example",
  "sort_title": "example",
  "extension": "gba",
  "directory_alias": "GBA",
  "media": {
    "thumbnail": "/mnt/SDCARD/Images/gba/example.png"
  },
  "metadata": {
    "source": "scan"
  }
}
```

Initial IDs can be hashes of normalized absolute paths. Later they can include
file hash, mtime, and size.

### ROM List Directory Browsing

The scanner cache remains a flat per-system `RomEntry` array. The FE ROM list
projects that cache at display time to show only ROM files and subdirectories
directly under the current directory.

- A enters a subdirectory, or launches a ROM file
- B returns to the parent directory inside subdirectories, or TOP at the alias root
- Directory ROM entries created by `scan_directories=true` systems are launch
  targets and are distinct from FE-only navigation directory entries
- The scanner cache format does not change; navigation entries are created only
  while rendering the FE ROM list

### Artwork Lookup

Thumbnails are resolved from the representative ROM path stored in `RomEntry`
using only the canonical per-system thumbnail root
`/mnt/SDCARD/Images/<system_id>`. Scraped images and user-provided images live
in the same directory. When a ROM lives in a subdirectory, preserve the path
relative to the ROM directory alias root when looking under the thumbnail
directory.

Example:

```text
rom alias root: /mnt/SDCARD/Roms/nes
thumbnail dir:  /mnt/SDCARD/Images/nes
rom path:       /mnt/SDCARD/Roms/nes/01/test.nes
relative stem:  01/test
```

Lookup priority:

```text
1. /mnt/SDCARD/Images/nes/01/test.png
2. /mnt/SDCARD/Images/nes/01/test.jpg
3. /mnt/SDCARD/Images/nes/01/test.jpeg
4. /mnt/SDCARD/Images/nes/01/test.webp
5. /mnt/SDCARD/Images/nes/test.png
6. /mnt/SDCARD/Images/nes/test.jpg
7. /mnt/SDCARD/Images/nes/test.jpeg
8. /mnt/SDCARD/Images/nes/test.webp
9. placeholder
```

Rules:

- `artwork.lookup` normally has exactly one directory per system.
- The scraper output, user-provided thumbnails, and frontend lookup all use
  `/mnt/SDCARD/Images/<system_id>`.
- Even for systems excluded from scraping, thumbnails manually placed under
  `/mnt/SDCARD/Images/<system_id>` are looked up and displayed normally.
- StockOS `/mnt/SDCARD/Imgs/*` and old lowercase `/mnt/SDCARD/images/*` paths
  are not normal lookup paths. Treat them only as importer/migration inputs if
  needed.
- The canonical extension list is lowercase `png`, `jpg`, `jpeg`, and `webp`;
  file lookup is case-insensitive.
- Prefer artwork that preserves the ROM subdirectory layout over flat artwork.
- Flat artwork remains as fallback for existing artwork and simple manual
  placement.
- Multi-file ROMs such as `.cue/.bin/.m3u` resolve thumbnails from the displayed
  `RomEntry` representative file stem, not from every file in the set.
- If no thumbnail is found, gallery mode shows a text fallback or the theme
  placeholder.

### Scraper Policy

`SystemDefinition.scraper` is future thumbnail-scraping policy. It is separate
from normal frontend thumbnail lookup, so systems with `scraper.enabled=false`
or no scraper field still display user-provided thumbnails normally.
`package/frontend/plumos/config/frontend/systems.json` includes a `scraper`
field for every system; runners and UI treat that field as the source of truth
for scraper eligibility.

Example:

```json
{
  "scraper": {
    "enabled": true,
    "reason": "simple_rom_crc",
    "extensions": ["nes", "unf", "unif", "zip"],
    "crc_workers": { "default": 1, "bulk": 2, "max": 2 },
    "download_workers": { "default": 2, "bulk": 3, "max": 4 }
  }
}
```

Rules:

- `crc_workers.default` is the normal UI initial value. Unvalidated systems use
  `1`.
- `crc_workers.bulk` is used for explicit bulk scraping. Unvalidated systems use
  `2`.
- `crc_workers.max` is the per-system cap determined by real A30 validation.
  Unvalidated systems use `2`.
- Validation may test `1..5`, but the normal FE UI must not exceed the
  per-system `max`.
- `download_workers` uses system-wide initial values of `default=2`, `bulk=3`,
  and `max=4` unless a later measurement says otherwise.
- Update per-system `crc_workers` only after saving
  `scripts/benchmark-a30-crc-workers.sh` results under `artifacts/`.
- Before doing CRC work, the scraper uses the same thumbnail lookup as the
  frontend. If an existing image is found, it returns `exists`.
- Existing images are not distinguished as user-provided versus
  scraper-downloaded and are not overwritten by default.
- Retry state for CRC misses and download failures lives under
  `/mnt/SDCARD/plumos/state/frontend/`; the `Images` directory contains images
  only.
- Scraper state includes relative path, size, mtime, and ctime so unchanged
  `no_match` files can be skipped before CRC work.
- Replacing a ROM with the same name and size still enters CRC work when mtime
  or ctime changes.
- Initial `scraper.reason` values are `simple_rom_crc`,
  `cd_media_crc_expensive`, `pc_disk_image_policy_pending`,
  `arcade_romset_policy_pending`, and `not_single_rom_crc`.
- `scraper.extensions` lists extensions the scraper actually CRCs. It may be
  narrower than the system-wide `extensions`.
- libretro DAT and thumbnail playlist mappings live in
  `config/frontend/scraper-sources.tsv`, separate from `systems.json`
  eligibility policy.
- Build-time preloaded cache lives under `share/frontend/artwork-scraper/`.
  Runtime fetched state/cache lives under
  `/mnt/SDCARD/plumos/cache/frontend/artwork-scraper`.

### `AppDefinition`

Apps/tools are separate from systems even if they appear near the TOP flow.

```json
{
  "id": "settings",
  "display_name": "Settings",
  "kind": "settings",
  "launch_profile": "internal:settings",
  "visible": true,
  "menu": "start"
}
```

`menu` controls placement. `start` means direct START menu, `apps` means the
Apps submenu, and `hidden` means an internal action that is not shown directly.

### `ThemeDefinition`

Themes are Graphic mode UI configuration and are separate from system definitions, layout
presets, and frontend behavior. The full schema lives in
[plumOS frontend theme model](frontend-theme-model.en.md). Stock theme formats
are not the official plumOS theme specification.

```json
{
  "version": 1,
  "id": "default",
  "target": "graphic",
  "display_name": "Default Graphic",
  "layout_preset": "grid_preview",
  "assets": {
    "font_ui": null,
    "font_fallback": "system_cjk",
    "background": null,
    "system_logo_root": "logos/systems",
    "system_icon_root": "icons/systems",
    "placeholder_thumbnail": null,
    "sound_effect_root": "sounds"
  },
  "colors": {
    "background": "#030404",
    "foreground": "#b8d0ca",
    "muted": "#85a6a6",
    "accent": "#ff850d",
    "panel": "#1f2b2e",
    "panel_inner": "#050808",
    "media_panel": "#1c2a2e",
    "selection_background": "#243a33",
    "selection_foreground": "#ffe67a"
  },
  "graphic_mode": {
    "top_layout": "tile_grid",
    "rom_layout": "list_preview",
    "transition": "slide",
    "transition_ms": 1000,
    "transition_axis": "vertical",
    "transition_easing": "smoothstep",
    "thumbnail_fit": "contain",
    "missing_thumbnail": "text_fallback"
  },
  "behavior_policy": {
    "theme_may_change_input": false,
    "theme_may_change_menu_actions": false,
    "theme_may_change_launch_profiles": false,
    "theme_may_change_rom_scan": false,
    "theme_may_change_resume": false
  }
}
```

Themes may control only Graphic mode colors, fonts, backgrounds, system icons, selection
style, spacing, thumbnail frames, and sound effects. They may not change button
behavior, START menu structure, SELECT core menu behavior, favorites, ROM
scanning, resume, or launch profiles. Text mode is not affected by theme
packages.

### `FrontendSettings`

```json
{
  "version": 1,
  "ui_mode": "text",
  "top_mode": "text",
  "rom_mode": "text",
  "show_empty_systems": false,
  "show_favorites_on_top": true,
  "show_recent_on_top": true,
  "boot_resume_mode": "off",
  "sort_systems": "sort_order",
  "sort_roms": "name",
  "rom_scan_policy": "on_enter",
  "rom_scan_slow_threshold_ms": 500,
  "rom_scan_test_file_count": 1000,
  "theme_id": "default",
  "graphic_theme_id": "default",
  "last_system_id": "gba"
}
```

`ui_mode` is the global default. `top_mode` and `rom_mode` may diverge.
The default `rom_scan_policy` is `on_enter`; stock-style manual refresh is not
the default. Use `cached` or `manual_refresh` only when performance requires it.

`boot_resume_mode`:

- `off`: show the normal TOP screen on boot.
- `last`: launch the pending resume session when one exists.
- `picker`: show the pending resume target plus recent list and let the user
  choose what to resume.

## TOP Screen

The TOP screen derives `HomeEntry` objects from `SystemDefinition`.

```json
{
  "kind": "system",
  "target_id": "nes",
  "display_name": "NES",
  "rom_count": 128,
  "thumbnail": null,
  "enabled": true
}
```

Rules:

- Show systems with ROMs by default.
- Show empty systems only when pinned.
- When `show_favorites_on_top=true`, show `Favorites` as a virtual TOP system.
- Use `display_name`.
- TOP shows `display_name`; ROM counts are shown on the ROM list side.
- Gallery mode may use system icons.
- Manufacturer names are not included in labels.
- The Favorites virtual system target id is `favorites`; its action is
  `internal:favorites`.

## ROM List

Text mode:

- One ROM per row.
- No icons or thumbnails.
- Up/down moves the cursor.
- Left/right can page or fast-jump.

Gallery mode:

- One ROM per screen.
- Show ROM title and thumbnail.
- Left/right moves previous/next.
- Up/down can jump by multiple entries.
- Missing thumbnails fall back to text.

The PFE prototype under `~/pfe/ui/file_list.py` implements a one-ROM horizontal
slide gallery. plumOS can use that interaction idea without adopting PFE's
config format or Python implementation as the specification.

### Refresh Policy

The ROM list re-scans the selected system's ROM directories whenever the user
enters that ROM list screen. Newly copied ROMs should appear without a manual
`Refresh ROM` action.

Initial policy:

- Avoid heavy full scans on the TOP screen.
- Run a full scan and reload TOP only when closing the START menu back to TOP,
  so newly copied ROMs appear without restarting the FE.
- Do not show ROM counts on the TOP screen.
- On ROM lists, show the count after the selected system scan/list load.
- Scan only the selected system when entering it.
- Show a text fallback such as `Scanning...` while scanning.
- Thumbnail loading/generation may be deferred until after the ROM list appears.
- If a 1000-file dummy ROM directory is too slow to browse comfortably, consider
  `cached` or `manual_refresh`.

Performance gate:

- Keep `on_enter` when first text-mode display for 1000 files is below 500 ms.
- If it exceeds 500 ms, prefer lightweight delta checks using directory mtime or
  entry count.
- Only consider stock-like manual refresh if that is still too slow.
- Record measurements and rationale in a decision record before switching to
  manual refresh.

On 2026-06-06, the A30 measured `374ms` ready time for 1000 dummy ROM files with
deferred thumbnail lookup via `--on-enter nes`, so the initial implementation
keeps `on_enter`.

`library-index.json` is a scan cache, but the filesystem remains the source of
truth for ROM lists.

## Favorites Model

Favorites are per-ROM user state. They are stored separately from the TOP system
list and are opened from `Favorites` in the START menu.

State path:

```text
/mnt/SDCARD/plumos/state/frontend/favorites.json
```

Schema:

```json
{
  "version": 1,
  "favorites": [
    {
      "system_id": "nes",
      "relative_path": "FC/example.nes",
      "title": "example",
      "file_name": "example.nes",
      "path": "/mnt/SDCARD/Roms/FC/example.nes",
      "thumbnail": "/mnt/SDCARD/Images/nes/example.png"
    }
  ]
}
```

Rules:

- A favorite is keyed by `system_id` plus the ROM `relative_path` from the ROM
  alias root.
- ROM lists show a text marker for favorite entries. Text mode does not depend
  on icons or thumbnails.
- The Favorites list can be displayed from the title/path snapshot stored in
  `favorites.json`.
- Moving or renaming a ROM does not automatically move the favorite. Add stale
  entry cleanup later as a settings/tool action.
- When `show_favorites_on_top=true`, Favorites may appear as a virtual TOP
  system.
- Do not adopt stock favorite formats directly. Add an importer only if needed.

## Recent / Resume Model

Recent and Resume are separate. Recent is the browsing history; Resume is the
single target that may be offered at the next boot. Keeping them separate avoids
mixing "played before" with "should be resumed next".

Recent state path:

```text
/mnt/SDCARD/plumos/state/frontend/recent.json
```

Schema:

```json
{
  "version": 1,
  "recents": [
    {
      "system_id": "gba",
      "relative_path": "GBA/example.gba",
      "title": "example",
      "file_name": "example.gba",
      "path": "/mnt/SDCARD/Roms/GBA/example.gba",
      "thumbnail": "/mnt/SDCARD/Images/gba/example.png",
      "launch_profile": "retroarch:mgba",
      "last_played_at": "2026-06-06T12:34:56Z",
      "resume_available": true
    }
  ]
}
```

Resume state path:

```text
/mnt/SDCARD/plumos/state/frontend/resume-session.json
```

Schema:

```json
{
  "version": 1,
  "pending": true,
  "reason": "shutdown",
  "system_id": "gba",
  "relative_path": "GBA/example.gba",
  "title": "example",
  "file_name": "example.gba",
  "path": "/mnt/SDCARD/Roms/GBA/example.gba",
  "thumbnail": "/mnt/SDCARD/Images/gba/example.png",
  "launch_profile": "retroarch:mgba",
  "updated_at": "2026-06-06T12:34:56Z",
  "auto_state_load": true
}
```

Rules:

- Adding a recent entry moves that ROM to the top.
- Store the resolved launch profile used for that launch.
- Resuming from history should prefer the `launch_profile` stored in
  recent/resume state over the current system default.
- `pending=true` in `resume-session.json` means the target should be offered on
  the next boot.
- `boot_resume_mode=last` launches pending resume directly. The text UI `boot`
  command prints only the decision and launch plan by default; `boot --execute`
  performs the launch.
- `boot_resume_mode=picker` shows pending resume plus recent list so the user can
  choose what to resume.
- For `retroarch:<core>` launch profiles, the plan is executable only when the
  plumOS RetroArch binary and matching `<core>_libretro.so` exist. Missing
  runtime files keep the launch plan non-executable.
- For `standalone:<emulator>` launch profiles, resolve to
  `/mnt/SDCARD/plumos/bin/plumos-standalone-launch <emulator> <ROM>`.
  `standalone:ppsspp`, `standalone:pcsx_rearmed`, `standalone:scummvm`,
  `standalone:easyrpg`, and `standalone:openbor` are A30 first-pass validated
  candidates.
- Pressing A in the ROM list calls `plumos-text-ui launch ... --execute`; it saves
  pending resume state before launch and clears it after the emulator returns.
- RetroArch Auto Save State / Auto Load State integration belongs to the later
  launcher/RetroArch implementation.
- While RetroArch is running, use a short power-button press rather than
  Function as the safe shutdown/resume trigger. On the A30 this is readable as
  `KEY_POWER` from `/dev/input/event0` (`axp22-supplyer`).
- While the frontend is blocked waiting for RetroArch, `plumos-safe-hotkeyd`
  watches the power-button event and `/dev/input/event3` (`gpio-keys-polled`)
  non-exclusively. The power button runs
  `plumos-safe-shutdown --shutdown --no-poweroff`, while `KEY_VOLUMEUP` /
  `KEY_VOLUMEDOWN` are handled like `plumos-volume-control up|down`.
  `plumos-text-ui launch --execute` auto-starts `plumos-safe-hotkeyd --oneshot`
  during RetroArch launches. Standalone emulators auto-start
  `plumos-safe-hotkeyd --volume-only`, which handles only the volume keys.
  Volume changes during gameplay update runtime softvol immediately and defer
  persistent settings writes until after the emulator exits. `SIGUSR1` remains
  available for tests of the RetroArch safe hotkey path without a physical
  button.

## SAFE Menu

The SAFE menu opened by Function is separate from START menu. START is for
normal frontend operations such as settings, apps, favorites, and recents. SAFE
is for game-time save/resume/exit operations.

Initial entries:

```json
{
  "id": "safe",
  "display_name": "SAFE",
  "entries": [
    { "id": "sleep", "display_name": "Sleep", "action": "safe:sleep" },
    { "id": "shutdown", "display_name": "Shutdown", "action": "safe:shutdown" },
    { "id": "cancel", "display_name": "Cancel", "action": "safe:cancel" }
  ]
}
```

Rules:

- Initial cursor is `Cancel` so pressing Function cannot immediately sleep or
  shut down by accident.
- `Sleep` flushes save RAM and keeps the resume candidate. If true suspend is
  not viable, fall back to pseudo-sleep.
- `Shutdown` saves state to the dedicated safe state slot 999, flushes save RAM,
  updates `resume-session.json`, exits RetroArch, runs `sync`, then powers off.
- `Cancel`, B, LEFT, and Function return to the previous screen.
- ROM-list A is connected to real launch. SAFE-menu sleep/shutdown actions are
  wired through `plumos-safe-shutdown --no-poweroff` for launcher/RetroArch save,
  exit, `sync`, and resume hold. `plumos-safe-shutdown` can select power and
  sleep backends, but real poweroff/suspend still needs a live-fire check.
- The direct RetroArch `plumos-safe-hotkeyd` safe-exit path is verified through
  text-ui launch auto-start and `.state999` creation. The in-game overlay menu
  still needs validation, but the direct trigger now belongs to the power button
  so Function can remain available to emulator-side menus.

## START Menu

Pressing START on the TOP screen or ROM list opens the system menu. Settings,
power, and non-emulator app flows should not clutter the TOP system list. On
devices like the A30 where stock UI has no reboot item, `Reboot` is hidden until
a safe reboot path is confirmed.

```json
{
  "id": "start",
  "display_name": "START",
  "entries": [
    { "id": "settings", "display_name": "Settings", "action": "internal:settings" },
    { "id": "apps", "display_name": "Apps", "action": "menu:apps" },
    { "id": "favorites", "display_name": "Favorites", "action": "internal:favorites" },
    { "id": "recent", "display_name": "Recent", "action": "internal:recent" },
    { "id": "refresh-current", "display_name": "Refresh Current System", "action": "scan:current" },
    { "id": "network", "display_name": "Network", "action": "internal:network" },
    { "id": "shutdown", "display_name": "Shutdown", "action": "system:shutdown", "confirm": true }
  ]
}
```

Rules:

- START menu must work in text mode.
- In gallery mode, START menu may appear as a text-list overlay.
- `Shutdown` requires a confirmation dialog. `Reboot` is shown only after a safe
  device-specific reboot path is confirmed.
- Non-emulator apps/tools live under an `Apps` submenu.
- `Refresh Current System` is a debug/safety action, not a normal requirement.
- START menu entries live in `menus.json`; app/tool definitions live in
  `apps.json`.
- The initial START menu id is `start`; the Apps submenu id is `apps`.

## Core Selection Model

When SELECT is pressed on a highlighted TOP system, the frontend opens a system
default profile picker using that system's `launch_profiles`. When SELECT is
pressed on a highlighted ROM entry, the frontend opens a per-ROM profile picker
that applies only to that ROM.

State path:

```text
/mnt/SDCARD/plumos/state/frontend/core-overrides.json
```

Schema:

```json
{
  "version": 1,
  "system_overrides": [
    {
      "system_id": "nes",
      "launch_profile": "retroarch:nestopia",
      "cpu_policy": "fixed",
      "cpu_freq_khz": 648000,
      "cpu_cores": 2
    }
  ],
  "rom_overrides": [
    {
      "system_id": "dos",
      "relative_path": "DOS/DOSBOX_DIGGER.ZIP",
      "launch_profile": "retroarch:dosbox_pure",
      "cpu_policy": "fixed",
      "cpu_freq_khz": 1344000,
      "cpu_cores": 4,
      "content_suffix": "#DIGGER.EXE",
      "audio_driver": "oss",
      "audio_latency_ms": 256,
      "dosbox_pure_force60fps": "true",
      "dosbox_pure_cycles": "max"
    }
  ]
}
```

Priority:

```text
1. ROM override
2. system override
3. SystemDefinition.default_launch_profile / default_cpu_policy
4. launcher default / auto detect
```

Rules:

- Store a `launch_profile` id, not a direct core path.
- A `launch_profile` override is valid only while it exists in the current
  `SystemDefinition.launch_profiles`; stale overrides are ignored and resolution
  falls through to the next priority.
- Profile ids such as `retroarch:fceumm` are resolved by the launcher into the
  RetroArch binary, core `.so`, config overrides, and CPU policy.
- Profile ids such as `standalone:ppsspp` are resolved into
  `plumos-standalone-launch`, an emulator id, the ROM path, and CPU policy.
- `cpu_policy` is `performance|fixed`; store `cpu_freq_khz` only for `fixed`.
  The user-facing UI does not show or save unpredictable `keep`.
- `cpu_cores` is `2|4`. On the A30, 2 cores means CPU0+CPU1 online, while
  4 cores means CPU0-CPU3 online.
- The SELECT core menu is the TOP/ROM shared `SCREEN_CORE_SELECT`; it shows the
  launch candidate with a runtime prefix, such as `Cores < RA: fceumm >`,
  `Cores < PICO: fceumm >`, or `Cores < SA: ppsspp >`, then below a separator shows
  `CPU freq < value >` and `CPU Cores < value >`. The `Default` row clears only
  the `launch_profile` override; ROM targets inherit the TOP/system override, and
  TOP targets inherit `default_launch_profile`. On TOP it writes a system
  override; on a ROM list it writes a ROM override through the same
  `plumos-text-ui core ... --set/--cpu --freq/--cores` save path.
- `Performance Settings` edits the same `core-overrides.json` system override.
  In the 2026-06-07 dummy-load measurement, 4-core performance load averaged
  about 4x the power of 2-core performance load, so core count is
  user-configurable.
- CPU settings use the same priority as profile selection:
  ROM override > system override > system default.
- A per-ROM override is keyed by `system_id` plus the ROM `relative_path` from
  the ROM alias root.
- For cores that select a startup file inside one content item, such as
  DOSBox-Pure, a per-ROM override can store a `content_suffix` such as
  `#DIGGER.EXE`. The launch plan appends it only at execution time, and direct
  `ROM.zip#EXE` launch targets fall back to the base ROM override.
- `audio_driver` and `audio_latency_ms` resolve to the RetroArch launcher's
  `--audio` and `--audio-latency` options. When unset, the launcher default is
  ALSA `default`; `oss` is used only as an explicit per-ROM compatibility
  fallback.
- `dosbox_pure_force60fps` and `dosbox_pure_cycles` are written to the launcher's
  temporary append config as DOSBox-Pure core options.
- Moving or renaming a ROM does not automatically move its per-ROM override.
- Clearing a ROM override falls back to the system override. Clearing the system
  override falls back to `default_launch_profile`.
- `default_launch_profile` is the plumOS recommended initial value, not a user
  override.
- Do not adopt stock `launchlist` directly. Migrate only useful candidates into
  plumOS launch profiles.

## Directory Discovery

```json
{
  "rom_roots": [
    "/mnt/SDCARD/Roms",
    "/mnt/SDCARD/roms"
  ],
  "artwork_roots": [
    "/mnt/SDCARD/Images"
  ]
}
```

Scan algorithm:

1. Read `systems.json`.
2. Resolve each `SystemDefinition.directory_aliases` against `rom_roots`.
3. Scan existing directories.
4. Create `RomEntry` objects for files with matching extensions.
5. For systems with `scan_directories=true`, also create a `RomEntry` for each
   first-level directory under the alias root and do not recursively scan inside
   that directory.
6. Route shared directories by extension.
7. Resolve artwork from each `RomEntry` representative path.
8. Save generated output to `state/frontend/library-index.json`.

### Directory ROM Sidecar

For systems with `scan_directories=true`, a directory itself becomes a
`RomEntry`. Some engines, such as ScummVM, also need an engine target id. Store
that id in a sidecar inside or next to the ROM directory.

Supported ScummVM sidecars:

```text
Roms/SCUMMVM/BASS-Floppy-1.3/.plumos-scummvm-target
Roms/SCUMMVM/BASS-Floppy-1.3/scummvm-target.txt
Roms/SCUMMVM/BASS-Floppy-1.3/.scummvm
Roms/SCUMMVM/BASS-Floppy-1.3.scummvm
Roms/SCUMMVM/BASS-Floppy-1.3.svm
```

The content may be `target=sky`, `gameid=sky`, `scummvm_target=sky`, or a
single-line `sky`. Target ids are limited to ASCII letters, digits, `.`, `_`,
and `-`. When no sidecar exists, the initial fallback remains `sky`, matching
the current validation ROM.

## Initial System Seed

The initial seed prioritizes systems that make sense on the A30. ROCKNIX
lowercase aliases come from `artifacts/reference/es_systems.cfg`; Miyoo/spruce
uppercase aliases use the spruceOS `Roms` tree as reference.

| `id` | display | Miyoo/spruce aliases | ROCKNIX aliases |
| --- | --- | --- | --- |
| `nes` | NES | `FC` | `nes`, `famicom` |
| `fds` | FDS | `FC`, `FDS` | `fds` |
| `sfc` | SFC | `SFC` | `sfc`, `snes` |
| `gb` | GB | `GB` | `gb` |
| `gbc` | GBC | `GB`, `GBC` | `gbc` |
| `gba` | GBA | `GBA` | `gba` |
| `megadrive` | Mega Drive | `MD` | `megadrive`, `genesis` |
| `mastersystem` | Master System | `MS`, `MD` | `mastersystem` |
| `gamegear` | Game Gear | `GG`, `GameGear`, `MD` | `gamegear` |
| `sega32x` | 32X | `THIRTYTWOX`, `MD` | `sega32x` |
| `segacd` | Sega CD | `SEGACD` | `segacd`, `megacd` |
| `pcengine` | PC Engine | `PCE` | `pcengine`, `tg16` |
| `pcenginecd` | PC Engine CD | `PCECD` | `pcenginecd`, `tg16cd` |
| `psx` | PlayStation | `PS` | `psx` |
| `psp` | PSP | `PSP`, `PPSSPP` | `psp` |
| `n64` | N64 | `N64` | `n64` |
| `dreamcast` | Dreamcast | `DC` | `dreamcast` |
| `saturn` | Saturn | `SATURN` | `saturn` |
| `neogeo` | Neo Geo | `NEOGEO` | `neogeo` |
| `neocd` | Neo Geo CD | `NEOCD` | `neocd` |
| `ngp` | NGP | `NGP` | `ngp` |
| `ngpc` | NGPC | `NGP`, `NGPC` | `ngpc` |
| `wonderswan` | WonderSwan | `WS`, `WSC` | `wonderswan` |
| `wonderswancolor` | WonderSwan Color | `WSC` | `wonderswancolor` |
| `arcade` | Arcade | `ARCADE` | `arcade` |
| `fbneo` | FBNeo | `FBNEO`, `ARCADE` | `fbneo` |
| `mame2003plus` | MAME 2003+ | `MAME2003PLUS`, `ARCADE` | `mame` |
| `cps1` | CPS1 | `CPS1`, `ARCADE` | `cps1` |
| `cps2` | CPS2 | `CPS2`, `ARCADE` | `cps2` |
| `cps3` | CPS3 | `CPS3`, `ARCADE` | `cps3` |
| `dos` | DOS | `DOS` | `pc` |
| `easyrpg` | EasyRPG | `EASYRPG` | `easyrpg` |
| `pico8` | PICO-8 | `PICO8`, `PICO` | `pico-8` |
| `scummvm` | ScummVM | `SCUMMVM` | `scummvm` |
| `openbor` | OpenBOR | `OPENBOR` | `openbor` |
| `ports` | Ports | `PORTS`, `A30PORTS` | `ports` |
| `media` | Media | `MEDIA`, `Video` | `mplayer` |
| `msx` | MSX | `MSX` | `msx` |
| `msx2` | MSX2 | `MSX2` | `msx2` |
| `pc88` | PC-88 | `PC88` | `pc-8800` |
| `pc98` | PC-98 | `PC98` | `pc-9800` |
| `x68000` | X68000 | `X68000` | `x68000` |
| `tic80` | TIC-80 | `TIC` | `tic-80` |

This is only a seed. Final system support and labels should be adjusted based
on A30 performance, available cores, and user experience.

## Open Questions

- Should shared directories such as `GB`/`GBC` be split on TOP or shown as one
  combined entry?
- Should arcade remain one `Arcade` entry or split into `FBNeo`, `MAME`, and
  `CPS1/2/3`?
- Should gallery TOP use a grid, or the same one-item slide model as ROM lists?
- Should `systems.json` be hand-written, or generated from a smaller
  `systems.seed.json` during build?

## References

- ROCKNIX/EmulationStation system/path source: `artifacts/reference/es_systems.cfg`
- Miyoo/spruce ROM directories: [spruceOS Roms](https://github.com/spruceUI/spruceOS/tree/main/Roms)
- PFE UI reference: `/Users/kroot/pfe/ui/main_menu.py`, `/Users/kroot/pfe/ui/file_list.py`
