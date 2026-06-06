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
- OS reboot, settings, network, tools, and non-emulator apps are reached through
  a START menu.
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
    thumbnails/
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
  "extensions": ["gen", "md", "smd", "32x", "bin", "chd", "zip", "7z"],
  "artwork": {
    "lookup": [
      { "root": "plumos", "path": "media/megadrive" },
      { "root": "sdcard", "path": "Imgs/MD" },
      { "root": "sdcard", "path": "Imgs/megadrive" }
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
- `extensions`: ROM extensions without dots.
- `artwork.lookup`: ordered thumbnail/cover lookup paths.
- `launch_profiles`: launcher-side candidates; the frontend does not execute
  cores directly.

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
    "thumbnail": "/mnt/SDCARD/Imgs/GBA/example.png"
  },
  "metadata": {
    "source": "scan"
  }
}
```

Initial IDs can be hashes of normalized absolute paths. Later they can include
file hash, mtime, and size.

### Artwork Lookup

Thumbnails are resolved from the representative ROM path stored in `RomEntry`.
When a ROM lives in a subdirectory, preserve the path relative to the ROM
directory alias root when looking under the artwork directory.

Example:

```text
rom alias root: /mnt/SDCARD/Roms/nes
artwork dir:    /mnt/SDCARD/images/nes
rom path:       /mnt/SDCARD/Roms/nes/01/test.nes
relative stem:  01/test
```

Lookup priority:

```text
1. /mnt/SDCARD/images/nes/01/test.png
2. /mnt/SDCARD/images/nes/01/test.jpg
3. /mnt/SDCARD/images/nes/01/test.jpeg
4. /mnt/SDCARD/images/nes/01/test.webp
5. /mnt/SDCARD/images/nes/test.png
6. /mnt/SDCARD/images/nes/test.jpg
7. /mnt/SDCARD/images/nes/test.jpeg
8. /mnt/SDCARD/images/nes/test.webp
9. placeholder
```

Rules:

- If `artwork.lookup` has multiple directories, try these candidates in
  definition order.
- The canonical extension list is lowercase `png`, `jpg`, `jpeg`, and `webp`;
  file lookup is case-insensitive.
- Prefer artwork that preserves the ROM subdirectory layout over flat artwork.
- Flat artwork remains as fallback for existing artwork and simple manual
  placement.
- Multi-file ROMs such as `.cue/.bin/.m3u` resolve thumbnails from the displayed
  `RomEntry` representative file stem, not from every file in the set.
- If no thumbnail is found, gallery mode shows a text fallback or the theme
  placeholder.

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

Themes are UI configuration and are separate from system definitions, layout
presets, and frontend behavior. The full schema lives in
[plumOS frontend theme model](frontend-theme-model.en.md). Stock theme formats
are not the official plumOS theme specification.

```json
{
  "version": 1,
  "id": "default",
  "display_name": "Default",
  "layout_preset": "compact_text",
  "assets": {
    "font_ui": "fonts/default.bdf",
    "font_fallback": "builtin",
    "background": null,
    "system_icon_root": "icons/systems",
    "placeholder_thumbnail": "images/placeholder.png",
    "sound_effect_root": "sounds"
  },
  "colors": {
    "background": "#101418",
    "foreground": "#f1f5f9",
    "accent": "#38bdf8",
    "selection_background": "#1f2937",
    "selection_foreground": "#ffffff"
  },
  "text_mode": {
    "force_no_icons": true,
    "line_height": 14,
    "show_thumbnail": false
  },
  "gallery_mode": {
    "transition": "slide",
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

Themes may control only colors, fonts, backgrounds, system icons, selection
style, spacing, thumbnail frames, and sound effects. They may not change button
behavior, START menu structure, SELECT core menu behavior, favorites, ROM
scanning, resume, or launch profiles. Text mode remains usable through built-in
font/color fallback even when theme assets or fonts are missing or broken.

### `FrontendSettings`

```json
{
  "version": 1,
  "ui_mode": "text",
  "top_mode": "text",
  "rom_mode": "text",
  "show_empty_systems": false,
  "show_favorites_on_top": false,
  "boot_resume_mode": "off",
  "sort_systems": "sort_order",
  "sort_roms": "name",
  "rom_scan_policy": "on_enter",
  "rom_scan_slow_threshold_ms": 500,
  "rom_scan_test_file_count": 1000,
  "theme_id": "default",
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
- Text mode shows only text and counts.
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
- TOP counts may use the previous scan cache.
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
      "thumbnail": "/mnt/SDCARD/images/nes/example.png"
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
      "thumbnail": "/mnt/SDCARD/images/gba/example.png",
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
  "thumbnail": "/mnt/SDCARD/images/gba/example.png",
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
- RetroArch Auto Save State / Auto Load State integration belongs to the later
  launcher/RetroArch implementation.

## START Menu

Pressing START on the TOP screen or ROM list opens the system menu. OS reboot,
settings, and non-emulator app flows should not clutter the TOP system list.

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
    { "id": "reboot", "display_name": "Reboot", "action": "system:reboot", "confirm": true },
    { "id": "shutdown", "display_name": "Shutdown", "action": "system:shutdown", "confirm": true }
  ]
}
```

Rules:

- START menu must work in text mode.
- In gallery mode, START menu may appear as a text-list overlay.
- `Reboot` and `Shutdown` require confirmation dialogs.
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
    { "system_id": "nes", "launch_profile": "retroarch:nestopia" }
  ],
  "rom_overrides": [
    {
      "system_id": "nes",
      "relative_path": "FC/example.nes",
      "launch_profile": "retroarch:fceumm"
    }
  ]
}
```

Priority:

```text
1. ROM override
2. system override
3. SystemDefinition.default_launch_profile
4. auto detect
```

Rules:

- Store a `launch_profile` id, not a direct core path.
- Profile ids such as `retroarch:fceumm` are resolved by the launcher into the
  RetroArch binary, core `.so`, config overrides, and CPU policy.
- A per-ROM override is keyed by `system_id` plus the ROM `relative_path` from
  the ROM alias root.
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
    "/mnt/SDCARD/plumos/media",
    "/mnt/SDCARD/Imgs",
    "/mnt/SDCARD/images"
  ]
}
```

Scan algorithm:

1. Read `systems.json`.
2. Resolve each `SystemDefinition.directory_aliases` against `rom_roots`.
3. Scan existing directories.
4. Create `RomEntry` objects only for matching extensions.
5. Route shared directories by extension.
6. Resolve artwork from each `RomEntry` representative path.
7. Save generated output to `state/frontend/library-index.json`.

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
- Should official artwork live under `plumos/media/<system>` or
  `Roms/<system>/media`?
- Should `systems.json` be hand-written, or generated from a smaller
  `systems.seed.json` during build?

## References

- ROCKNIX/EmulationStation system/path source: `artifacts/reference/es_systems.cfg`
- Miyoo/spruce ROM directories: [spruceOS Roms](https://github.com/spruceUI/spruceOS/tree/main/Roms)
- PFE UI reference: `/Users/kroot/pfe/ui/main_menu.py`, `/Users/kroot/pfe/ui/file_list.py`
