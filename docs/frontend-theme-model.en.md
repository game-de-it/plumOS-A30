# plumOS Frontend Graphic Theme Model

This document defines the first plumOS frontend Graphic mode theme model. It
uses an EmulationStation-like package concept where a theme owns the visual
presentation for Graphic mode. Stock theme formats are not the official plumOS
theme specification. If needed, stock compatibility can be considered later as
an importer.

## Policy

- Themes control Graphic mode presentation only.
- Text mode does not use theme colors, layouts, assets, or fonts.
- `settings.json` uses `graphic_theme_id` as the selected Graphic theme.
- `theme_id` is still saved temporarily for compatibility, but new code should
  use `graphic_theme_id`.
- User changes made in `Theme Settings` are stored as `settings.json` overrides:
  `graphic_top_layout`, `graphic_transition`, `graphic_transition_ms`,
  `graphic_transition_axis`, and `graphic_transition_easing`. Do not rewrite the
  theme JSON package for those edits.
- Themes cannot change button mappings, menu actions, launch profiles, ROM scan,
  resume, CPU/Wi-Fi, or power policy.
- If a theme requests behavior control, it is blocked and the built-in Graphic
  fallback is used.

## Layout

```text
/mnt/SDCARD/plumos/
  config/frontend/themes.json
  themes/
    default/
      theme.json
      fonts/
      logos/systems/
      icons/systems/
      images/
      sounds/
```

`graphic_theme_id` resolves to
`/mnt/SDCARD/plumos/themes/<graphic_theme_id>/theme.json`. If loading fails,
Graphic mode remains usable through built-in color/font fallback.

## Asset Spec

- Background image: PNG, recommended `640x480`, displayed across the screen.
- System logo: PNG, recommended `256x96` or larger, displayed in the Graphic TOP
  tile media area.
- ROM preview placeholder: PNG, recommended `280x156`, used when no thumbnail
  exists.
- System icon: future use, PNG, square, recommended `128x128` or larger.
- Font asset: future use. For now, Graphic mode prefers the TTF/CJK fallback.
- Text mode bitmap fonts are not part of theme packages.

## System Logo Targets

System logos are required for systems where `systems.json` has `enabled != false`,
plus the `favorites` and `recent` virtual TOP entries. Graphic TOP resolves those
virtual entries through the same `logos/systems/<id>.png` path as normal systems.

Systems with `enabled:false` may remain as future or investigation definitions,
but they are excluded from normal TOP scans, launch profile audits, and required
theme logo counts.

The older `all-logo.png` sheet was generated from the 40-system seed, so it is
not the current source of truth for supported systems. Use the audit script to
check the current target count and missing logos.

```sh
scripts/audit-system-logos.py
scripts/audit-system-logos.py --list-targets
```

As of 2026-06-12, the default theme family has 49 target systems and 2 virtual
TOP logos, for 51 required logos total. All required logos are present.
`dreamcast`, `n64`, and `saturn` are excluded from required logos because they
are `enabled:false`, but their older seed logos may remain as optional assets.

Use this script to regenerate the filled-in and virtual TOP logos. It writes to
each `logos/systems/` directory under `default`, `default-horizontal`, and
`default-vertical`.

```sh
scripts/generate-missing-system-logos.py
```

## `themes.json`

```json
{
  "version": 1,
  "themes": [
    {
      "id": "default",
      "display_name": "Default Graphic",
      "path": "/mnt/SDCARD/plumos/themes/default/theme.json",
      "builtin_fallback": true,
      "target": "graphic"
    }
  ]
}
```

## `theme.json`

Themes may control:

- Graphic TOP / ROM preview colors
- Graphic mode fonts
- backgrounds
- system icons
- selection style
- spacing
- thumbnail frames
- sound effects

Themes may not control:

- button mappings
- START menu structure or actions
- SELECT core menu behavior
- favorite/recent/resume state formats
- ROM scan policy
- launch profiles or RetroArch/core selection
- CPU/Wi-Fi/power policy
- Text mode console design

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
    "selection_foreground": "#ffe67a",
    "danger": "#ff140a"
  },
  "graphic_mode": {
    "top_layout": "tile_grid",
    "rom_layout": "list_preview",
    "transition": "slide",
    "transition_ms": 1000,
    "transition_axis": "vertical",
    "transition_easing": "smoothstep",
    "thumbnail_fit": "contain",
    "thumbnail_frame": "simple",
    "missing_thumbnail": "text_fallback"
  },
  "spacing": {
    "screen_padding": 8,
    "row_gap": 2,
    "column_gap": 10
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

`graphic_mode.transition` changes presentation only. The currently supported
values are `none` and `slide`. On Graphic TOP, `slide` moves whole pages with
eased vertical motion. `transition_ms` is clamped to `80..1000`; when `slide`
omits it, `260ms` is the baseline. Themes may not change input mappings,
page size, confirm/back behavior, or menu actions. `transition_axis` supports
`vertical` / `horizontal`, and `transition_easing` supports `smoothstep` /
`linear`.
`top_layout` supports `tile_grid` and `tile_strip`. `tile_grid` shows six
entries in a 3x2 grid; `tile_strip` shows two entries in one row for horizontal
scroll themes.
Overrides saved by `Theme Settings` are applied after reading theme JSON.
Changing `Theme` clears those overrides so the newly selected package starts
from its own defaults.

`behavior_policy` is a safety check. If any value is `true`, the controller UI
treats the theme as requesting behavior control, blocks that request, and uses
the built-in Graphic fallback.
