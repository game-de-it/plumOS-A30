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
    themes.json               available theme definitions
    settings.json             frontend settings
  state/frontend/
    library-index.json        generated scan cache
    recent.json               plumOS recent list
    favorites.json            plumOS favorites
    play-history.json         play count and last played timestamps
    core-overrides.json       per-ROM/per-system launch profile overrides
    cursor-state.json         last selected system/ROM/view position
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

### `AppDefinition`

TOP 画面とは別の app/tool menu に出す項目です。system と混ぜても良いですが、data model
上は分けます。

```json
{
  "id": "settings",
  "display_name": "Settings",
  "kind": "settings",
  "launch_profile": "internal:settings",
  "visible": true
}
```

### `ThemeDefinition`

theme は UI 表示の設定であり、system 定義とは分離します。

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

text mode では `use_icons=false` を強制できます。つまり theme に icon があっても、
text mode では使いません。

### `FrontendSettings`

ユーザー設定です。

```json
{
  "version": 1,
  "ui_mode": "text",
  "top_mode": "text",
  "rom_mode": "text",
  "show_empty_systems": false,
  "sort_systems": "sort_order",
  "sort_roms": "name",
  "theme_id": "default",
  "last_system_id": "gba"
}
```

`ui_mode` は全体 default です。`top_mode` と `rom_mode` を分けて保存することで、
TOP は text、ROM list は gallery のような構成も可能にします。

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
- display name は `display_name` を使う
- text mode では `display_name` と count だけを表示する
- gallery mode では `display_name` と system icon を使ってよい
- メーカー名は表示名に含めない

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
6. artwork lookup を実行する
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
