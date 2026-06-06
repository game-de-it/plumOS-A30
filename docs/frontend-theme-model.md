# plumOS frontend theme model

この文書は plumOS frontend theme の初期仕様です。stock theme format を plumOS の正式仕様には
しません。必要になった場合だけ、後で importer として検討します。

## 配置

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

`settings.json` の `theme_id` から `/mnt/SDCARD/plumos/themes/<theme_id>/theme.json` を読みます。
読み込みに失敗した場合、text mode は built-in font/color fallback で必ず操作できる状態に
戻します。

## 読み込み方針

- theme は見た目だけを扱う
- layout preset は theme から参照できるが、button 操作や menu action は変えられない
- text mode は icon、thumbnail、font asset が壊れていても text だけで動く
- gallery mode は ROM thumbnail を使う。見つからない場合は theme placeholder、さらに
  text fallback の順に戻す
- artwork lookup は `systems.json` の `artwork.lookup` が source of truth
- plumOS 公式 artwork の初期配置は `/mnt/SDCARD/plumos/media/<system>` とし、既存 artwork は
  `/mnt/SDCARD/Imgs/<alias>` と `/mnt/SDCARD/images/<system>` を移動せず fallback として読む

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

theme が扱えるもの:

- colors
- fonts
- background
- system icons
- selection style
- spacing
- thumbnail frame
- sound effects

theme が扱えないもの:

- button mapping
- START menu 構造や action
- SELECT core menu の挙動
- favorite/recent/resume の state format
- ROM scan policy
- launch profile や RetroArch/core 選択
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

`behavior_policy` は安全確認用です。ここに `true` がある theme は controller UI では
behavior control を要求したものとして block し、text fallback を使います。
