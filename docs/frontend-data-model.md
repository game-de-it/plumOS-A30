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
- OS reboot、settings、network、tools、emulator 以外の app は START menu から辿れる
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
    thumbnails/               generated/cached thumbnail images
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

Field:

- `id`: plumOS 内部 ID。lowercase ASCII。state/history/launch profile の key に使う
- `display_name`: UI 表示名。短い機種名
- `short_name`: さらに短い表示名。小さい画面や status 表示用
- `directory_aliases`: ROM directory 名の候補
- `extensions`: この機種に属する ROM 拡張子。dot は付けない
- `artwork.lookup`: thumbnail/capture/cover の探索先。順番に見る
- `launch_profiles`: 起動候補。実体は frontend ではなく launcher 側で解決する

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
    "thumbnail": "/mnt/SDCARD/Imgs/GBA/example.png"
  },
  "metadata": {
    "source": "scan"
  }
}
```

`id` は path だけに依存させず、将来的には file hash や mtime/size を使える形にします。
初期実装は normalized absolute path の hash で十分です。

### Artwork lookup

thumbnail は `RomEntry` の代表 ROM path から解決します。ROM が subdirectory にある場合は、
ROM directory alias root からの相対 path を artwork directory 側にも反映します。

例:

```text
rom alias root: /mnt/SDCARD/Roms/nes
artwork dir:    /mnt/SDCARD/images/nes
rom path:       /mnt/SDCARD/Roms/nes/01/test.nes
relative stem:  01/test
```

lookup priority:

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

rules:

- `artwork.lookup` に複数 directory がある場合は、定義順に上の候補を試す
- extension の canonical list は lowercase の `png`, `jpg`, `jpeg`, `webp` とし、
  file 探索時は case-insensitive に扱う
- subdirectory 構造を保った画像を flat 配置より優先する
- flat 配置は既存 artwork や簡単な手動配置の fallback として扱う
- `.cue/.bin/.m3u` のような multi-file ROM は、file 単体ではなく表示対象の
  `RomEntry` 代表 file stem から thumbnail を探す
- 見つからない場合、gallery mode は text fallback または theme の placeholder を表示する

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

theme は UI 表示の設定であり、system 定義、layout preset、frontend behavior とは分離します。
詳細な schema は [plumOS frontend theme model](frontend-theme-model.md) に分けます。
stock theme format は plumOS の正式仕様にしません。

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

theme が扱えるのは color、font、background、system icon、selection 表現、spacing、
thumbnail frame、sound effect だけです。button 操作、START menu、SELECT core menu、
favorite、ROM scan、resume、launch profile は theme から変更できません。
text mode は theme asset/font が壊れていても built-in font/color fallback で操作可能にします。

### `FrontendSettings`

ユーザー設定です。

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
- text mode では `display_name` と count だけを表示する
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
- TOP 画面の count は前回 scan cache の値を使ってよい
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
      "thumbnail": "/mnt/SDCARD/images/nes/example.png"
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
      "thumbnail": "/mnt/SDCARD/images/gba/example.png",
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
  "thumbnail": "/mnt/SDCARD/images/gba/example.png",
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
- RetroArch の Auto Save State / Auto Load State との実接続は launcher/RetroArch 実装時に行う
- RetroArch 実行中の safe shutdown/resume menu は、電源ボタンではなく Function button を
  第一候補にする。電源ボタンは kernel 側で処理される可能性があるため、必須経路にしない

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
- `Shutdown` は save state、save RAM flush、`resume-session.json` 更新、RetroArch 終了、
  `sync`、poweroff の順に行う
- `Cancel`、B、LEFT、Function は元の画面へ戻る
- 初期 prototype では plan preview だけを表示し、実処理は launcher/RetroArch 実装後に接続する

## START menu model

TOP 画面や ROM list 画面で START を押すと、system menu を開きます。OS reboot や
settings など、emulator ではない機能は TOP に直接並べず、この menu から辿ります。

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

表示 rules:

- START menu は text mode でも必ず使える
- gallery mode 中でも START menu は text list として overlay 表示してよい
- `Reboot` と `Shutdown` は confirm dialog を必須にする
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

優先順位:

```text
1. ROM override
2. system override
3. SystemDefinition.default_launch_profile
4. auto detect
```

rules:

- 保存する値は core path ではなく `launch_profile` id とする
- `retroarch:fceumm` のような profile id は launcher 側で RetroArch binary、core `.so`、
  config override、CPU policy へ解決する
- per-ROM override の key は `system_id` と、ROM alias root からの `relative_path`
  の組み合わせとする
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
    "/mnt/SDCARD/plumos/media",
    "/mnt/SDCARD/Imgs",
    "/mnt/SDCARD/images"
  ]
}
```

scan algorithm:

1. `systems.json` を読む
2. 各 `SystemDefinition.directory_aliases` を `rom_roots` に対して解決する
3. 存在する directory を scan する
4. `extensions` に一致する file だけを `RomEntry` にする
5. 共有 directory では拡張子で system を振り分ける
6. `RomEntry` の代表 path から artwork lookup を実行する
7. 結果を `state/frontend/library-index.json` に保存する

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
- artwork の正式配置を `plumos/media/<system>` にするか、`Roms/<system>/media` にするか
- `systems.json` を手書き JSON にするか、build 時に `systems.seed.json` から生成するか

## 参考

- ROCKNIX/EmulationStation 系 system/path: `artifacts/reference/es_systems.cfg`
- Miyoo/spruce 系 ROM directory: [spruceOS Roms](https://github.com/spruceUI/spruceOS/tree/main/Roms)
- PFE UI reference: `/Users/kroot/pfe/ui/main_menu.py`, `/Users/kroot/pfe/ui/file_list.py`
