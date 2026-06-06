# plumOS Frontend Theme Model

This document defines the first plumOS frontend theme model. Stock theme formats
are not the official plumOS theme specification. If needed, stock compatibility
can be considered later as an importer.

## Layout

```text
/mnt/SDCARD/plumos/
  config/frontend/themes.json
  themes/
    default/
      theme.json
      fonts/
      icons/systems/
      images/
      sounds/
```

`settings.json` selects a theme through `theme_id`, which resolves to
`/mnt/SDCARD/plumos/themes/<theme_id>/theme.json`. If loading fails, text mode
must remain usable through built-in font/color fallback.

## Loading Policy

- Themes control presentation only.
- A theme may refer to a layout preset, but cannot change button behavior or
  menu actions.
- Text mode works without icons, thumbnails, or font assets.
- Gallery mode uses ROM thumbnails. Missing thumbnails fall back to the theme
  placeholder, then to text.
- Artwork lookup is owned by `systems.json` `artwork.lookup`.
- Official plumOS artwork starts under `/mnt/SDCARD/plumos/media/<system>`.
  Existing artwork under `/mnt/SDCARD/Imgs/<alias>` and
  `/mnt/SDCARD/images/<system>` is read in place as fallback.

## `themes.json`

```json
{
  "version": 1,
  "themes": [
    {
      "id": "default",
      "display_name": "Default",
      "path": "/mnt/SDCARD/plumos/themes/default/theme.json",
      "builtin_fallback": true
    }
  ]
}
```

## `theme.json`

Themes may control:

- colors
- fonts
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
    "muted": "#94a3b8",
    "accent": "#38bdf8",
    "selection_background": "#1f2937",
    "selection_foreground": "#ffffff",
    "danger": "#f97373"
  },
  "text_mode": {
    "force_no_icons": true,
    "line_height": 14,
    "cursor": ">",
    "show_thumbnail": false
  },
  "gallery_mode": {
    "transition": "slide",
    "thumbnail_fit": "contain",
    "thumbnail_frame": "simple",
    "missing_thumbnail": "text_fallback"
  },
  "spacing": {
    "screen_padding": 8,
    "row_gap": 2,
    "column_gap": 6
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

`behavior_policy` is a safety check. If any value is `true`, the controller UI
treats the theme as requesting behavior control, blocks that request, and uses
text fallback.
