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

Themes are UI configuration and are separate from system definitions.

```json
{
  "id": "default",
  "display_name": "Default",
  "fonts": {
    "ui": "/mnt/SDCARD/plumos/frontend/fonts/default.bdf"
  },
  "palette": "dark",
  "icons": {
    "system_icon_root": "/mnt/SDCARD/plumos/frontend/icons"
  },
  "text_mode": {
    "use_icons": false,
    "line_height": 14
  },
  "gallery_mode": {
    "transition": "slide",
    "thumbnail_fit": "contain"
  }
}
```

Text mode can force `use_icons=false`; icons may exist but are not required.

### `FrontendSettings`

```json
{
  "version": 1,
  "ui_mode": "text",
  "top_mode": "text",
  "rom_mode": "text",
  "show_empty_systems": false,
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
- Use `display_name`.
- Text mode shows only text and counts.
- Gallery mode may use system icons.
- Manufacturer names are not included in labels.

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

`library-index.json` is a scan cache, but the filesystem remains the source of
truth for ROM lists.

## START Menu

Pressing START on the TOP screen or ROM list opens the system menu. OS reboot,
settings, and non-emulator app flows should not clutter the TOP system list.

```json
{
  "id": "system-menu",
  "entries": [
    { "id": "settings", "display_name": "Settings", "action": "internal:settings" },
    { "id": "apps", "display_name": "Apps", "action": "internal:apps" },
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
6. Resolve artwork.
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
