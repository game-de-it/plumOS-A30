# plumOS frontend data model

この文書は、plumOS frontend の独自 data model の初期設計です。stock MainUI の
`config.json` は参考情報として読みますが、この model の正式仕様にはしません。

## 目標

- TOP 画面は対応機種一覧を表示する
- 機種名は短くする。例: `NES`, `GB`, `Mega Drive`
- `Nintendo - FC` や `SEGA - MD` のようなメーカー名付き表記は使わない
- Miyoo 系の大文字省略 directory と、ROCKNIX/EmulationStation 系の lowercase
  directory の両方を認識する
- text mode では TOP/ROM/settings などを text だけで操作できる
- gallery mode では画像を使う。ROM list は 1 画面に 1 ROM と thumbnail を表示し、
  左右でスライドする
- 機種を選択したタイミングで ROM directory を読み直し、追加した ROM がすぐ表示される
- settings、network、power、tools、emulator 以外の app は START menu から辿れる
- 既存 SD カードの ROM/artwork は移動せずに読めるようにする
- RetroArch/core 起動、CPU policy、save/state 配置は別 profile で管理する

## 設計原則

- 機種は `SystemDefinition` として定義する
- ROM directory 名は機種そのものではなく `DirectoryAlias` として扱う
- 1 つの機種は複数 directory alias を持てる
- 1 つの directory alias を複数機種で共有できる
- 共有 alias は拡張子で振り分ける。例: Miyoo `GB` directory から `GB` と `GBC`
  を分離できる
- TOP 画面の表示順は `SystemDefinition.sort_order` で決める
- 機種一覧には、ROM が存在する機種だけを出すのを default にする
- `pinned=true` の機種は ROM がなくても表示できる
- UI mode と library data は分離する。text mode は image/icon がなくても必ず動く

## Runtime directory

```text
/mnt/SDCARD/plumos/
  config/frontend/
    systems.json              human-maintained system definitions
    apps.json                 app/tool menu definitions
    menus.json                START menu and submenu definitions
    themes.json               available theme definitions
    settings.json             frontend settings
  state/frontend/
    library-index.json        generated scan cache
    recent.json               plumOS recent list
    favorites.json            plumOS favorites
    play-history.json         play count and last played timestamps
    resume-session.json       pending boot resume target
    core-overrides.json       per-ROM/per-system launch profile overrides
    cursor-state.json         last selected system/ROM/view position
    scan-stats.json           scan timing and fallback policy state
  cache/frontend/
    render-cache/             optional disposable resized render cache
    text-index/               optional search/sort cache
  frontend/
    themes/
    icons/
    fonts/
```

`systems.json` は人間が管理する source of truth です。`library-index.json` は scan
結果なので生成物として扱います。

## Core entities

### `SystemDefinition`

機種の定義です。TOP 画面に出る単位です。

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

Field:

- `id`: plumOS 内部 ID。lowercase ASCII。state/history/launch profile の key に使う
- `display_name`: UI 表示名。短い機種名
- `short_name`: さらに短い表示名。小さい画面や status 表示用
- `directory_aliases`: ROM directory 名の候補
- `scan_directories`: true の場合、alias root 直下の directory を ROM entry として扱う。
  ScummVM/EasyRPG のように game が directory 単位で配布される system 用
- `extensions`: この機種に属する ROM 拡張子。dot は付けない
- `artwork.lookup`: thumbnail/capture/cover の探索先。通常は
  `/mnt/SDCARD/Images/<system_id>` の 1 経路だけにする
- `scraper`: optional。thumbnail scraper 対象可否と CRC/download worker 数の system 別
  policy を持つ。未指定の場合は scraper 対象外または global default として扱う
- `launch_profiles`: 起動候補。実体は frontend ではなく launcher 側で解決する

### Launch profile

`launch_profiles` と `default_launch_profile` は `kind:id` の文字列で指定します。現時点の
FE は以下を扱います。

- `retroarch:<core_id>`: `/mnt/SDCARD/plumos/retroarch/cores/<core_id>_libretro.so`
  を RetroArch launcher で起動する
- `picoarch:<core_id>`: 同じ libretro core を軽量 frontend の PicoArch で起動する
- `standalone:<emulator_id>`: `/mnt/SDCARD/plumos/emulators/<emulator_id>/` 配下の
  standalone launcher で起動する
- `pyxel:a30`: Pyxel launcher で `.pyxapp` / `.py` を起動する

`picoarch:<core_id>` は libretro core を共有しますが、実行経路は RetroArch ではなく
`/mnt/SDCARD/plumos/bin/plumos-picoarch-launch` です。CPU policy/core 数は system または
override の値を `PLUMOS_PICOARCH_*` 環境変数として渡します。
Core Settings では、`systems.json` にある各 `retroarch:<core_id>` から対応する
`picoarch:<core_id>` を検証用候補として自動追加します。初期 default は
`default_launch_profile` のままとし、PicoArch を使う場合はユーザーが明示的に選びます。
ただし実機検証で表示、操作、実速度が成立しない core は候補から除外します。
2026-06-16 時点では `tgbdual` を GB/GBC の通常候補から外し、PicoArch 自動追加対象からも
除外します。PC Engine 系の `mednafen_pce` は実動作負荷が高いため RA/PICO とも通常候補から
除外し、`mednafen_pce_fast` を使います。

### `DirectoryAlias`

ROM directory 名の別名です。

```json
{
  "name": "FC",
  "source": "miyoo",
  "priority": 10,
  "shared": true
}
```

`name` は実ディレクトリ名です。Miyoo 系は `FC`, `MD`, `WS` のような大文字省略形、
ROCKNIX/EmulationStation 系は `nes`, `megadrive`, `wonderswan` のような lowercase
名を登録します。

`shared=true` の alias は複数機種で使ってよいことを示します。例:

- `GB` directory を `gb` と `gbc` が共有し、`.gb` と `.gbc` で振り分ける
- `FC` directory を `nes` と `fds` が共有し、`.nes` と `.fds` で振り分ける
- `MD` directory を `megadrive`, `mastersystem`, `gamegear`, `sega32x` が共有する
  可能性があるため、拡張子で振り分ける

### `RomEntry`

scan 結果として生成される ROM 単位の entry です。手書きしません。

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

`id` は path だけに依存させず、将来的には file hash や mtime/size を使える形にします。
初期実装は normalized absolute path の hash で十分です。

### ROM list directory browsing

scanner cache は従来どおり system ごとの flat な `RomEntry` 配列として保持します。
FE の ROM list は cache を表示する時点で、現在の directory 直下にある ROM file と
subdirectory だけに投影します。

- A は subdirectory ではその directory へ移動し、ROM file では起動する
- B は subdirectory 内では親 directory へ戻り、alias root では TOP へ戻る
- `scan_directories=true` の system で生成される directory ROM entry は起動対象であり、
  ROM list navigation 用の仮想 directory entry とは区別する
- scanner cache format は変更しない。directory navigation entry は FE 表示時だけ作る

### Artwork lookup

thumbnail は `RomEntry` の代表 ROM path から、system ごとの canonical thumbnail root
`/mnt/SDCARD/Images/<system_id>` だけを見て解決します。scraper が取得した画像とユーザーが
手で置いた画像は同じ directory に保存します。ROM が subdirectory にある場合は、ROM
directory alias root からの相対 path を thumbnail directory 側にも反映します。

例:

```text
rom alias root: /mnt/SDCARD/Roms/nes
thumbnail dir:  /mnt/SDCARD/Images/nes
rom path:       /mnt/SDCARD/Roms/nes/01/test.nes
relative stem:  01/test
```

lookup priority:

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

rules:

- `artwork.lookup` は通常 1 system につき 1 directory だけにする
- scraper の保存先、ユーザー手動配置、frontend lookup はすべて
  `/mnt/SDCARD/Images/<system_id>` に揃える
- scraper 対象外の system でも、ユーザーが `/mnt/SDCARD/Images/<system_id>` に配置した
  thumbnail は通常通り lookup して表示する
- stockOS 由来の `/mnt/SDCARD/Imgs/*` や旧 lowercase `/mnt/SDCARD/images/*` は
  通常 lookup には使わず、必要になった場合だけ importer/migration の入力として扱う
- extension の canonical list は lowercase の `png`, `jpg`, `jpeg`, `webp` とし、
  file 探索時は case-insensitive に扱う
- subdirectory 構造を保った画像を flat 配置より優先する
- flat 配置は既存 artwork や簡単な手動配置の fallback として扱う
- `.cue/.bin/.m3u` のような multi-file ROM は、file 単体ではなく表示対象の
  `RomEntry` 代表 file stem から thumbnail を探す
- 見つからない場合、gallery mode は text fallback または theme の placeholder を表示する

### Scraper Policy

`SystemDefinition.scraper` は将来の thumbnail scraping 用 policy です。frontend の通常
thumbnail lookup とは独立しており、`scraper.enabled=false` や未指定の system でも
ユーザー手動配置の thumbnail は表示します。
`package/frontend/plumos/config/frontend/systems.json` では全 system に `scraper` field を置き、
runner/UI はこの field を scraper 対象可否の正本として扱います。

例:

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

rules:

- `crc_workers.default` は通常UIの初期値。未検証 system は `1`
- `crc_workers.bulk` はユーザーが明示した一括取得時の値。未検証 system は `2`
- `crc_workers.max` は実機検証で決める system 別上限。未検証 system は `2`
- 検証用には `1..5` を測ってよいが、FE の通常UIで `max` を超えた値は使わない
- `download_workers` は system 共通の初期値 `default=2`, `bulk=3`, `max=4` を基本にする
- system 別 `crc_workers` は A30 実機の `scripts/benchmark-a30-crc-workers.sh` 結果を
  `artifacts/` に残してから更新する
- scraper はまず frontend と同じ thumbnail lookup を行い、既存画像があれば CRC 計算せず
  `exists` として扱う
- 既存画像はユーザー手動配置と scraper 取得画像を区別せず、default では上書きしない
- CRC miss や download failure の再試行制御は `/mnt/SDCARD/plumos/state/frontend/` 配下の
  scraper state に保存し、`Images` directory には画像以外の管理 file を置かない
- scraper state は relative path, size, mtime, ctime を持ち、同一 file と判断できる
  `no_match` は CRC 前に skip できるようにする
- 同名・同サイズの ROM 再配置でも、mtime または ctime が変わった場合は CRC 対象にする
- `scraper.reason` は `simple_rom_crc`, `cd_media_crc_expensive`,
  `pc_disk_image_policy_pending`, `arcade_romset_policy_pending`,
  `not_single_rom_crc` を初期値として使う
- `scraper.extensions` は scraper が実際に CRC 対象にする拡張子。system 全体の
  `extensions` より狭くてよい
- libretro DAT と thumbnail playlist の対応は
  `config/frontend/scraper-sources.tsv` に置き、`systems.json` の対象可否とは分けて管理する
- build 時の事前取得 cache は `share/frontend/artwork-scraper/` に入り、実機側の取得済み
  state/cache は `/mnt/SDCARD/plumos/cache/frontend/artwork-scraper` に置く

### `AppDefinition`

TOP 画面とは別の app/tool menu に出す項目です。system と混ぜても良いですが、data model
上は分けます。

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

`menu` は表示先です。`start` は START menu 直下、`apps` は Apps submenu、`hidden` は
直接表示しない内部 action を示します。

### `ThemeDefinition`

theme は Graphic mode 表示の設定であり、system 定義、layout preset、frontend behavior とは分離します。
詳細な schema は [plumOS frontend theme model](frontend-theme-model.md) に分けます。
stock theme format は plumOS の正式仕様にしません。

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
    "gallery_background": null,
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

theme が扱えるのは Graphic mode の color、font、background、system icon、selection 表現、spacing、
thumbnail frame、sound effect だけです。button 操作、START menu、SELECT core menu、
favorite、ROM scan、resume、launch profile は theme から変更できません。
Text mode は theme の影響を受けません。

Gallery list の背景は `assets.gallery_background` を最優先し、未指定の場合は theme directory
の `images/rom_gallery_background.png`、`images/rom-gallery-background.png`、
`images/gallery_background.png`、`images/gallery-background.png`、`images/games_background.png`
を順に探します。`games_background.png` は RetroFE theme 由来素材を取り込みやすくするための
互換 fallback です。

### `FrontendSettings`

ユーザー設定です。

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

`ui_mode` は全体 default です。`top_mode` と `rom_mode` を分けて保存することで、
TOP は text、ROM list は gallery のような構成も可能にします。

`rom_scan_policy` の default は `on_enter` です。stock FE のような「手動 refresh しないと
ROM が出ない」方式は default にしません。性能上必要な場合だけ `cached` または
`manual_refresh` へ切り替えます。

`boot_resume_mode`:

- `off`: 起動時は通常の TOP を表示する
- `last`: pending resume session があれば同じ ROM/launch profile で再開する
- `picker`: 起動時に pending resume と recent list を表示し、ユーザーが選んで再開する

## TOP screen model

TOP 画面は `SystemDefinition` から `HomeEntry` を生成して表示します。

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

表示 rules:

- default は ROM が存在する system だけ表示する
- `pinned=true` の system は ROM がなくても表示する
- `show_favorites_on_top=true` の場合、`Favorites` を仮想 system として TOP に表示する
- display name は `display_name` を使う
- TOP では `display_name` を表示し、ROM 数は ROM list 側で表示する
- gallery mode では `display_name` と system icon を使ってよい
- メーカー名は表示名に含めない
- Favorites 仮想 system の target id は `favorites`、action は `internal:favorites` とする

## ROM list model

ROM list は `RomEntry` を表示します。

text mode:

- 1 行 1 ROM
- icon/thumbnail は使わない
- 上下で cursor 移動
- 左右は page up/down または fast jump

gallery mode:

- 1 画面 1 ROM
- ROM title と thumbnail を表示
- 左右で previous/next
- 上下は 5 件 jump などの fast navigation に使える
- thumbnail がない場合は text fallback を表示する

PFE の `~/pfe/ui/file_list.py` では、gallery mode が 1 ROM/1 screenshot の横 slide
として実装されています。plumOS でもこの操作感は参考にしますが、PFE の config format
や Python 実装をそのまま仕様にはしません。

### Refresh policy

ROM list は機種を選択して ROM list screen に入るたびに、その機種の ROM directory を
再 scan します。これにより、SD カードへ ROM を追加した直後でも、手動の「Refresh ROM」
操作なしで一覧に反映されます。

初期方針:

- TOP 画面では重い full scan をしない
- UI Settings の `Refresh TOP` を実行した時だけ full scan と TOP reload を行い、
  追加された ROM を FE 再起動なしで反映する。実行中は最低1秒の更新中画面を表示する
- TOP 画面には ROM 数を表示しない
- ROM list 側では、機種選択時の scan/list load 後の件数を表示する
- 機種選択時に対象 system だけを scan する
- scan 中は `Scanning...` のような text fallback を出す
- 画像 thumbnail の生成/読み込みは ROM list 表示後に遅延してよい
- 1000 file の dummy ROM directory で操作不能になるほど遅い場合は、`cached` または
  `manual_refresh` 方式を検討する

性能判定:

- 1000 file の text mode 初回表示が 500ms 未満なら `on_enter` を維持
- 500ms 以上なら、directory mtime/entry count を使った軽量差分 check を優先
- それでも遅い場合だけ、stock FE に近い manual refresh 方式を候補にする
- manual refresh 方式へ変更する場合は、理由と計測結果を decision record に残す

2026-06-06 の A30 実機計測では、thumbnail lookup を遅延した `--on-enter nes` の
1000 dummy ROM ready time は `374ms` でした。このため初期実装では `on_enter` を維持します。

`library-index.json` は scan cache ですが、freshness より速さが必要な場面だけ使います。
ROM list の正本は常に filesystem です。

## Favorites model

Favorites は ROM 単位の user state です。TOP の system list とは分離して保存し、START menu
の `Favorites` から一覧を開けるようにします。

保存先:

```text
/mnt/SDCARD/plumos/state/frontend/favorites.json
```

schema:

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

rules:

- favorite の key は `system_id` と ROM alias root からの `relative_path` の組み合わせ
  とする
- ROM list では favorite entry に text marker を表示する。text mode では icon や
  thumbnail に依存しない
- Favorites 一覧は `favorites.json` に保存した title/path snapshot から表示できる
- ROM を移動/rename した場合、favorite は自動追跡しない。後で stale entry cleanup を
  settings/tool として用意する
- `show_favorites_on_top=true` の場合、Favorites は TOP に仮想 system として表示できる
- stock favorite 形式を直接採用せず、必要な場合だけ importer を作る

## Recent / Resume model

Recent と Resume は分離します。Recent は履歴一覧、Resume は次回起動時の再開候補です。
混ぜると「過去に遊んだ」ことと「次回自動再開すべき」ことの意味が曖昧になるため、
別 file にします。

Recent 保存先:

```text
/mnt/SDCARD/plumos/state/frontend/recent.json
```

schema:

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

Resume 保存先:

```text
/mnt/SDCARD/plumos/state/frontend/resume-session.json
```

schema:

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

rules:

- recent entry は最後に起動した ROM を先頭へ移動する
- recent entry の `launch_profile` は起動時に解決済みの profile を保存する
- 履歴から再開する場合は、現在の system default ではなく recent/resume に保存された
  `launch_profile` を優先する
- `resume-session.json` の `pending=true` は「次回起動時に再開候補として扱う」ことを示す
- `boot_resume_mode=last` は pending resume を直接起動する。text UI の `boot` は default
  では判断と launch plan だけを表示し、`boot --execute` の時だけ実行する
- `boot_resume_mode=picker` は pending resume と recent list を表示し、ユーザー選択で再開する
- launch profile が `retroarch:<core>` の場合は、plumOS 配下の RetroArch binary と
  `<core>_libretro.so` が存在する場合だけ実行可能にする。欠けている場合は launch plan を
  non-executable として扱う
- launch profile が `standalone:<emulator>` の場合は、
  `/mnt/SDCARD/plumos/bin/plumos-standalone-launch <emulator> <ROM>` に解決する。
  `standalone:ppsspp`、`standalone:pcsx_rearmed`、`standalone:scummvm`、
  `standalone:easyrpg`、`standalone:openbor` は A30 実機 first-pass
  検証済みの候補として扱う
- ROM list で A を押すと `plumos-text-ui launch ... --execute` を呼び、起動前に
  pending resume を保存し、エミュレータ終了後に pending resume を clear する
- RetroArch の Auto Save State / Auto Load State との実接続は launcher/RetroArch 実装時に行う
- RetroArch 実行中の safe shutdown/resume trigger は Function ではなく電源ボタン短押しを
  使う。A30 では `/dev/input/event0` (`axp22-supplyer`) から `KEY_POWER` として読める。
- FE が RetroArch 待ちでブロックされる間は、`plumos-safe-hotkeyd` が電源ボタン event と
  `/dev/input/event3` (`gpio-keys-polled`) を非排他で監視する。電源ボタンから
  `plumos-safe-shutdown --shutdown --no-poweroff` を起動し、`KEY_VOLUMEUP` /
  `KEY_VOLUMEDOWN` は `plumos-volume-control up|down` 相当として扱う。
  `plumos-text-ui launch --execute` は RetroArch launch 中に
  `plumos-safe-hotkeyd --oneshot` を自動起動する。standalone emulator では
  `plumos-safe-hotkeyd --volume-only` を自動起動し、音量キーだけを扱う。
  ゲーム中の音量変更は runtime softvol だけを即時反映し、永続設定への保存は
  emulator 終了後に遅延する。`SIGUSR1` trigger は引き続き RetroArch safe hotkey path の
  物理ボタンなし実機試験に使える。

## SAFE menu model

Function button で開く SAFE menu は、START menu と役割を分けます。START menu は OS 設定、
Apps、Favorites、Recent など通常 frontend 操作用、SAFE menu はゲーム中の保存・再開・終了用です。

初期 entry:

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

rules:

- 初期 cursor は `Cancel` に置き、Function の誤操作で即 sleep/shutdown しない
- `Sleep` は save RAM flush と resume candidate 維持を行う。実 suspend が難しい場合は
  疑似 sleep 方針へ fallback する
- `Shutdown` は dedicated safe state slot 999 への save state、save RAM flush、
  `resume-session.json` 更新、RetroArch 終了、`sync`、poweroff の順に行う
- `Cancel`、B、LEFT、Function は元の画面へ戻る
- ROM list の A は実起動へ接続済み。SAFE menu の sleep/shutdown は
  `plumos-safe-shutdown --no-poweroff` 経由で launcher/RetroArch 側の保存・終了・`sync`・
  resume hold まで接続済み。`plumos-safe-shutdown` は power backend と sleep backend を
  選べるが、実 poweroff/suspend 発火は別途確認する
- RetroArch 実行中の `plumos-safe-hotkeyd` 直接安全終了pathは、text-ui launch 中の
  自動起動と `.state999` 作成まで確認済み。ゲーム中 overlay menu は未完了だが、
  直接 trigger は電源ボタンへ寄せ、Function は emulator 側 menu と競合させない

## START menu model

TOP 画面や ROM list 画面で START を押すと、system menu を開きます。settings や
power など、emulator ではない機能は TOP に直接並べず、この menu から辿ります。
A30 のように stock UI が reboot を持たない機種では、安全な再起動手順を確認するまで
`Reboot` entry は出しません。

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

表示 rules:

- START menu は text mode でも必ず使える
- gallery mode 中でも START menu は text list として overlay 表示してよい
- `Shutdown` は confirm dialog を必須にする。`Reboot` は機種ごとの安全な手順が確認できた場合のみ出す
- emulator 以外の app/tool は `Apps` submenu に入れる
- `Refresh Current System` は debug/保険用であり、通常操作で必須にしない
- START menu の entry は `menus.json`、app/tool 定義は `apps.json` に置く
- 初期実装の START menu id は `start`、Apps submenu id は `apps` とする

## Core selection model

TOP 画面で system に cursor が合っている状態で SELECT を押すと、その system の
`launch_profiles` から system default profile を選びます。ROM list で ROM に cursor が
合っている状態で SELECT を押すと、その ROM だけに適用する profile を選びます。

保存先:

```text
/mnt/SDCARD/plumos/state/frontend/core-overrides.json
```

schema:

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

優先順位:

```text
1. ROM override
2. system override
3. SystemDefinition.default_launch_profile / default_cpu_policy
4. launcher default / auto detect
```

rules:

- 保存する値は core path ではなく `launch_profile` id とする
- `launch_profile` override は現在の `SystemDefinition.launch_profiles` に存在する
  場合だけ有効とし、候補から外れた古い override は無視して次の優先順位へ fallback する
- `retroarch:fceumm` のような profile id は launcher 側で RetroArch binary、core `.so`、
  config override、CPU policy へ解決する
- `standalone:ppsspp` のような profile id は `plumos-standalone-launch` と
  emulator id、ROM path、CPU policy へ解決する
- `cpu_policy` は `performance|fixed`。`fixed` のときだけ `cpu_freq_khz` を保存する。
  ユーザー向け UI では予測しづらい `keep` を表示・保存しない
- `cpu_cores` は `2|4`。A30では2コアは CPU0+CPU1、4コアは CPU0-CPU3 を online にする
- SELECT core menu は TOP/ROM 共通の `SCREEN_CORE_SELECT` で、起動候補を
  `Cores < RA: fceumm >` / `Cores < PICO: fceumm >` /
  `Cores < SA: ppsspp >` のように実行経路の省略接頭辞付きで表示し、区切り線の下に `CPU freq < value >` と
  `CPU Cores < value >` を表示する。`Default` 行は `launch_profile` override だけを消し、
  ROM では TOP/system override を、TOP では `default_launch_profile` を継承する。
  TOPではsystem override、ROM listではROM overrideを
  `plumos-text-ui core ... --set/--cpu --freq/--cores` の同じ保存経路へ書く。
- `Performance Settings` も同じ `core-overrides.json` のsystem overrideを編集する。
  2026-06-07 の dummy負荷測定では 4コアperformance負荷は2コアperformance負荷の
  約4倍の平均電力になったため、core数もユーザー設定対象にする
- CPU設定も `launch_profile` と同じく ROM override > system override > system default の順で解決する
- per-ROM override の key は `system_id` と、ROM alias root からの `relative_path`
  の組み合わせとする
- DOSBox-Pure など1つのcontent内で起動ファイルを選ぶcoreでは、per-ROM override の
  `content_suffix` に `#DIGGER.EXE` のような suffix を保存できる。FE launch plan は
  実行時だけ ROM path に suffix を足し、`ROM.zip#EXE` を直接指定した場合も base ROM の
  override を fallback 参照する
- `audio_driver` と `audio_latency_ms` は RetroArch launcher の `--audio` /
  `--audio-latency` に解決する。未指定時は launcher 既定の ALSA `default` を使い、
  `oss` はROM別の互換 fallback として明示した場合だけ使う
- `dosbox_pure_force60fps` と `dosbox_pure_cycles` は DOSBox-Pure core option として
  launcher の一時 append config に書き込む
- ROM file を移動/rename した場合、per-ROM override は自動追跡しない
- ROM override を clear すると system override へ戻り、system override もなければ
  `default_launch_profile` へ戻る
- `default_launch_profile` は plumOS 推奨初期値であり、ユーザー設定ではない
- stock `launchlist` をそのまま採用せず、必要な候補だけ plumOS launch profile へ
  migration する

## Directory discovery

scan root は複数持てます。

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

scan algorithm:

1. `systems.json` を読む
2. 各 `SystemDefinition.directory_aliases` を `rom_roots` に対して解決する
3. 存在する directory を scan する
4. `extensions` に一致する file を `RomEntry` にする
5. `scan_directories=true` の system では alias root 直下の directory も `RomEntry`
   にし、その directory 内部は再帰 scan しない
6. 共有 directory では拡張子で system を振り分ける
7. `RomEntry` の代表 path から artwork lookup を実行する
8. 結果を `state/frontend/library-index.json` に保存する

### Directory ROM sidecar

`scan_directories=true` の system では、directory 自体を `RomEntry` として扱います。
ScummVM のように directory とは別に engine target id が必要な場合は、ROM directory
または隣接 path に sidecar を置きます。

対応する ScummVM sidecar:

```text
Roms/SCUMMVM/BASS-Floppy-1.3/.plumos-scummvm-target
Roms/SCUMMVM/BASS-Floppy-1.3/scummvm-target.txt
Roms/SCUMMVM/BASS-Floppy-1.3/.scummvm
Roms/SCUMMVM/BASS-Floppy-1.3.scummvm
Roms/SCUMMVM/BASS-Floppy-1.3.svm
```

内容は `target=sky`、`gameid=sky`、`scummvm_target=sky`、または `sky` のような
1行形式を受け付けます。target id は ASCII の英数字、`.`、`_`、`-` のみ許可します。
sidecar が無い場合、初期 fallback は既存検証ROMに合わせて `sky` です。

## Initial system seed

初期 seed は A30 で現実的に使う機種を優先します。ROCKNIX の lowercase 名は
`artifacts/reference/es_systems.cfg` を、Miyoo/spruce 系の大文字名は spruceOS の
`Roms` directory を参考にします。

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

この表は初期 seed です。採用 system や表示名は、A30 の性能、core availability、
ユーザー体験を見て調整します。

## Open questions

- `GB`/`GBC` のような共有 directory を TOP で分けて見せるか、統合 entry にするか
- arcade 系を `Arcade` に統合するか、`FBNeo`, `MAME`, `CPS1/2/3` を分けるか
- gallery mode の TOP を grid にするか、ROM list と同じ 1 item slide に寄せるか
- `systems.json` を手書き JSON にするか、build 時に `systems.seed.json` から生成するか

## 参考

- ROCKNIX/EmulationStation 系 system/path: `artifacts/reference/es_systems.cfg`
- Miyoo/spruce 系 ROM directory: [spruceOS Roms](https://github.com/spruceUI/spruceOS/tree/main/Roms)
- PFE UI reference: `/Users/kroot/pfe/ui/main_menu.py`, `/Users/kroot/pfe/ui/file_list.py`
