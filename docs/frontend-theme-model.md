# plumOS frontend Graphic theme model

この文書は plumOS frontend の Graphic mode theme 初期仕様です。EmulationStation
のように、theme package が画面の見た目をまとめて持つ考え方を採用します。ただし
stock theme format を plumOS の正式仕様にはしません。必要になった場合だけ、後で
importer として検討します。

## 基本方針

- theme は Graphic mode の見た目だけを扱う。
- Text mode は theme の color、layout、asset、font を使用しない。
- `settings.json` では `graphic_theme_id` が選択中の Graphic theme を表す。
- 互換のため `theme_id` は当面保存するが、新規実装では `graphic_theme_id` を使う。
- theme は button mapping、menu action、launch profile、ROM scan、resume、CPU/Wi-Fi/power
  policy を変更できない。
- theme が behavior control を要求した場合は block し、built-in Graphic fallback を使う。

## 配置

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

`graphic_theme_id` から `/mnt/SDCARD/plumos/themes/<graphic_theme_id>/theme.json`
を読みます。読み込みに失敗した場合でも、Graphic mode は built-in color/font fallback で
操作できる状態に戻します。

## 素材仕様

- 背景画像: PNG、推奨 `640x480`、画面全体に表示される。
- system logo: PNG、推奨 `256x96` 以上。Graphic TOP の tile media area に表示される。
- ROM preview placeholder: PNG、推奨 `280x156`、thumbnail が無い時に表示される。
- system icon: 将来用。推奨 PNG、正方形 `128x128` 以上。
- font asset: 将来用。現時点では Graphic mode の TTF/CJK fallback を優先する。
- Text mode 用 bitmap font は theme package では扱わない。

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

theme が扱えるもの:

- Graphic TOP / ROM preview の colors
- Graphic mode 用 fonts
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
- Text mode の console design

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
    "transition_ms": 220,
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

`graphic_mode.transition` は見た目だけを変える設定です。現時点の正式値は `none` と
`slide` です。Graphic TOP の `slide` はページ全体が上下に入れ替わる演出です。
`transition_ms` は `80..500` の範囲で扱い、未指定時の `slide` は `220ms` を基準にします。
theme は入力割り当て、ページサイズ、決定/戻る動作を変更できません。

`behavior_policy` は安全確認用です。ここに `true` がある theme は controller UI では
behavior control を要求したものとして block し、built-in Graphic fallback を使います。
