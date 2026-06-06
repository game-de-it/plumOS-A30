# Frontend prototype

最初の plumOS frontend は、画面描画を始める前の stock inventory scanner です。A30 上で
stock SD カード構成を読み、`Emu`, `RApp`, `App`, `Themes` の `config.json` と、そこから
参照される ROM/artwork/metadata の存在を確認します。

この scanner は stock 仕様を plumOS frontend の新仕様として採用するためのものでは
ありません。既存仕様を観察し、移行/互換/破棄の判断材料を集めるためのものです。
`config.json` の位置づけは [Stock frontend inventory](stock-frontend-inventory.md) に
まとめます。

## build

```sh
./scripts/docker-build.sh frontend
```

生成物:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/bin/plumos-library-scan
dist/plumos-frontend/plumos/bin/plumos-text-ui
dist/plumos-frontend/plumos/bin/plumos-controller-ui
dist/plumos-frontend/plumos/config/frontend/systems.json
dist/plumos-frontend/plumos/config/frontend/menus.json
dist/plumos-frontend/plumos/config/frontend/apps.json
dist/plumos-frontend/plumos/config/frontend/themes.json
dist/plumos-frontend/plumos/config/frontend/settings.json
dist/plumos-frontend/plumos/themes/default/theme.json
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

## deploy

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

## manual run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

manual mode は `0` で終了します。wrapper から起動される通常 mode では、現時点では
stock MainUI へ fallback するために `75` で終了します。
通常 mode は stdout を抑制し、詳細は `/mnt/SDCARD/plumos/logs/plumos-frontend.log`
へ記録します。必要な場合は `PLUMOS_FRONTEND_STDOUT=1` で stdout へも出力します。

## plumOS library scan

`plumos-library-scan` は plumOS 独自の `systems.json` を読み、Miyoo/ROCKNIX directory
alias を使って ROM を scan し、`library-index.json` を生成する prototype です。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-library-scan'
```

default path:

```text
systems: /mnt/SDCARD/plumos/config/frontend/systems.json
full output:      /mnt/SDCARD/plumos/state/frontend/library-index.json
on-enter output:  /mnt/SDCARD/plumos/state/frontend/systems/<system>.json
roms:    /mnt/SDCARD/Roms, /mnt/SDCARD/roms
```

`--on-enter nes` を指定すると、対象 system だけを scan し、全体 cache を壊さず
`state/frontend/systems/nes.json` に保存します。ROM list に入る瞬間の再 scan はこの動作を
使います。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-library-scan --on-enter nes'
```

`--on-enter` では text mode の初回表示を優先し、thumbnail lookup は default で遅延します。
thumbnail も同時に解決したい場合は `--with-thumbnails` を使います。

環境変数:

- `PLUMOS_SDCARD_ROOT`: SD card root。default は `/mnt/SDCARD`
- `PLUMOS_ROOT`: plumOS root。default は `$PLUMOS_SDCARD_ROOT/plumos`
- `PLUMOS_SYSTEMS_JSON`: system 定義 file
- `PLUMOS_LIBRARY_INDEX`: 出力先 cache file

実装済み:

- `systems.json` の初期 seed
- Miyoo 大文字 alias と ROCKNIX lowercase alias の scan
- subdirectory を含む recursive ROM scan
- ROM extension filter
- `RomEntry` 生成
- ROM alias root からの相対 path を保持した thumbnail lookup
- subdirectory artwork 優先、flat artwork fallback、placeholder fallback
- `png`, `jpg`, `jpeg`, `webp` の case-insensitive lookup
- `Roms` と `roms`、`GBA` と `gba` のような大文字小文字違いの重複 scan 抑制
- `--on-enter` の per-system cache 出力
- text mode 初回表示向けの deferred thumbnail lookup

## ROM scan benchmark

1000 dummy ROM files の実機計測は以下の script で実行します。dummy file は
`/mnt/SDCARD/plumos/tmp/rom-scan-bench` だけに作られ、計測後に削除されます。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/benchmark-a30-rom-scan.sh 1000
```

2026-06-06 の A30 実機結果:

```text
system nes roms=1000 thumbnails=0
timing load_ms=10 scan_ms=362 sort_ms=2 ready_ms=374 write_ms=29 total_ms=403
summary alias_dirs=1 files_seen=1000 matched=1000 roms=1000 thumbnails=0 elapsed_ms=403
```

`ready_ms=374` なので、text mode の初回表示については 500ms 基準を下回りました。
このため、現時点では stock FE 方式の manual refresh へ戻さず、on-enter scan を維持します。

## plumOS text UI

`plumos-text-ui` は、画面描画/input 実装へ入る前に system list と ROM list の data flow を
確認するための SSH 向け text UI prototype です。現時点では実機の MainUI replacement として
自動起動しません。

TOP 表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui top'
```

`top` は既存の `library-index.json` を読みます。cache がなければ full scan を実行します。
明示的に更新したい場合は `top --refresh` を使います。
`settings.json` の `show_favorites_on_top=true` の場合、`Favorites` を仮想 system として
TOP に表示します。仮想 entry は `internal:favorites` へ遷移します。

ROM list 表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui roms ports --limit 10'
```

`roms <system>` は内部で `plumos-library-scan --on-enter <system>` を実行し、
`state/frontend/systems/<system>.json` を読んで一覧表示します。これは将来の
「機種選択時に毎回ROM listを読み込む」動作の最小prototypeです。

START menu 表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui menu start'
```

Apps submenu 表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui menu apps'
```

`menu start` は `menus.json` を読み、settings/apps/favorites/network/reboot/shutdown へ
辿る menu model を表示します。`Reboot` と `Shutdown` は `confirm=yes` として定義します。
この prototype は action を表示するだけで、reboot/shutdown は実行しません。
`menu apps` は `apps.json` の `menu=apps` entry を表示します。

System core 選択表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core system nes'
```

System default core/profile を保存:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core system nes --set retroarch:nestopia'
```

ROM 別 core/profile override を保存:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui core rom nes FC/example.nes --set retroarch:fceumm'
```

`core system` は TOP 画面で system に cursor が合っている状態の SELECT に相当します。
`core rom` は ROM list で ROM に cursor が合っている状態の SELECT に相当します。
保存先は `state/frontend/core-overrides.json` です。保存する値は core `.so` path ではなく
`launch_profile` id です。優先順位は
`ROM override > system override > default_launch_profile > auto detect` です。
`--clear` を指定すると対象の override を削除して fallback へ戻します。

Favorite toggle:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui favorite rom ports "PORTS/Start SSH.sh" --toggle'
```

Favorites 一覧:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui favorites'
```

TOP の仮想 system から Favorites 一覧へ入る動作:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui roms favorites'
```

`favorite rom` は ROM list で favorite toggle を押す操作に相当します。保存先は
`state/frontend/favorites.json` です。ROM list 表示の `Fav` 列では favorite entry に
`*` を表示します。START menu の `Favorites` entry は、この Favorites 一覧へ辿るための
入口です。`show_favorites_on_top=true` の場合は、START menu だけでなく TOP からも
Favorites へ入れます。

Recent 追加と一覧:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui recent add ports "PORTS/Start SSH.sh" --profile external:port'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui recent'
```

Resume session 設定と表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui resume set ports "PORTS/Start SSH.sh" --profile external:port --reason shutdown'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui resume show'
```

起動時 resume 判断:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui boot'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-text-ui boot --execute'
```

`recent.json` は履歴一覧、`resume-session.json` は次回起動時の再開候補です。
`settings.json` の `boot_resume_mode` は `off`, `last`, `picker` を指定できます。
`boot` は default では判断と launch plan を表示するだけです。`--execute` を付けた場合だけ、
`boot_resume_mode=last` かつ pending resume session がある時に同じ ROM/launch profile を
実行します。RetroArch profile は `/mnt/SDCARD/plumos/retroarch/bin/retroarch` と
`/mnt/SDCARD/plumos/retroarch/cores/<core>_libretro.so` が存在する場合だけ実行可能です。
実際の Auto State Load は RetroArch/launcher 実装時に接続します。

`plumos-frontend` は boot mode で起動した時に
`/mnt/SDCARD/plumos/bin/plumos-text-ui boot --execute` を一度呼びます。既定値は
`boot_resume_mode=off` なので、通常は TOP 表示へ進むだけです。

## plumOS controller UI

`plumos-controller-ui` は controller-first の最小 prototype です。まだ framebuffer/SDL の
画面描画は行わず、SSH stdout に TOP/ROM list/START menu/Favorites/Recent/Settings の
状態を描き、`/dev/input/event*` または stdin fallback から入力を受けます。A30 では
`/proc/bus/input/devices` から `gpio-keys-polled` を探し、通常は `/dev/input/event3` を
自動選択します。
`plumos-input-compare` では、stock `keymon` と stock `MainUI` が動作中でも
`/dev/input/event3` を非排他で直接 open/poll できることを確認しています。

TOP を 1 回だけ表示:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --once --no-clear'
```

script 入力で状態遷移を確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script down,a,b,select,start,q'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script start,a,b,start,down,down,a,b,start,down,down,down,a,b,q'
```

実機ボタンの raw event を確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --dump-events --timeout 10'
```

操作:

- D-pad: cursor 移動
- A/right: TOP では ROM list へ入る。ROM list では launch preview を出す
- B/left: ROM list、Favorites、Recent、Settings から TOP へ戻る。START menu では元の画面へ戻る
- START: START menu を開く
- START menu: Settings/Favorites/Recent は実画面へ遷移し、その他は action preview を出す
- SELECT: system/per-ROM core preview
- Settings: 現在値を一覧表示し、A は edit preview を出す
- SSH stdin fallback: `w/s/a/d`, `e` または space, `b`, `m`, `c`, `q`

Settings 画面では plumOS frontend 設定と theme 状態に加えて、A30 固有設定の
read-only inventory も表示します。現在は `/config/system.json` から volume、
brightness、display color、Wi-Fi enabled flag、keymap、language、stock theme、
CPU mode を読み、`/tmp/wpa_status.txt` から redacted Wi-Fi runtime status だけを
読みます。SSID/PSK は読みません。write-enabled controls は backend 検証後に別途
実装します。

確認例:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script start,a,q | grep -E "A30|Wi-Fi|Volume|Brightness|Keymap|Input"'
```

2026-06-06 の A30 実機確認:

```text
plumOS text UI - TOP
No.  System                 ROMs  Default profile
  1. Ports                     2  external:port

plumOS text UI - ROM list
system: ports
ready_ms: 10
  1.     Start SSH                          PORTS/Start SSH.sh
  2.     Stop SSH                           PORTS/Stop SSH.sh

plumOS text UI - menu
menu: start
  1. Settings                 internal   internal:settings        no
  2. Apps                     submenu    menu:apps                no
  3. Favorites                internal   internal:favorites       no
  4. Recent                   internal   internal:recent          no
  5. Network                  internal   internal:network         no
  6. Refresh Current System   scan       scan:current             no
  7. Reboot                   system     system:reboot            yes
  8. Shutdown                 system     system:shutdown          yes

plumOS text UI - core selection
scope: system
system: nes (NES)
current: retroarch:fceumm (plumOS default)
  1. retroarch:fceumm               yes      no       -        *
  2. retroarch:nestopia             no       no       -

plumOS text UI - Favorites
count: 0
```

## 現在読む情報

- `/mnt/SDCARD/Emu/*/config.json`
- `/mnt/SDCARD/RApp/*/config.json`
- `/mnt/SDCARD/App/*/config.json`
- `/mnt/SDCARD/Themes/*/config.json`
- `rompath` が指す ROM directory の存在、通常 file 数、`extlist` 一致数
- `imgpath` が指す artwork directory の存在と画像 file 数
- `gamelist` が指す file の存在と size
- `launchlist` entry 数
- `/mnt/SDCARD/Roms/recentlist.json` の存在確認、size、非空行 entry 数

現時点で読む field:

- `label`
- `name`
- `launch`
- `rompath`
- `imgpath`
- `extlist`
- `gamelist`
- `launchlist`

`rompath`, `imgpath`, `gamelist` は `config.json` のある directory から相対解決します。
`extlist` は大文字小文字を区別せずに一致確認します。現時点では ROM/artwork の file 名は
log に出さず、件数だけを記録します。

## 実機確認

2026-06-06 に A30 上で manual mode を実行し、以下を確認しました。

```text
summary configs emu=18 rapp=27 app=5 themes=42
summary roms dirs=25 files=0 matched=0
summary artwork dirs=41 images=4027
summary metadata gamelists=0 launchers=22
```

`Roms/recentlist.json` は `size=234 entries=3` として検出しました。現状は値を出力せず、
非空行数だけを entry 数として扱っています。

観察:

- 現在の SD カードでは、多くの configured ROM directory は存在しますが、ROM file は
  ほぼ空です。
- `Imgs` 側には artwork が入っており、既存 artwork を移動せずに参照できる可能性が
  高いです。
- `RApp/mednafen_wswan/config.json` の `imgpath` は `../..Imgs/WSC` になっており、
  `../../Imgs/WSC` の typo と思われます。stock 互換では、このような設定ミスも
  検出・補正対象にします。

## 次に足すもの

- ROM file と artwork file の名前対応 rule
- `gamelist` XML の parse
- `recentlist.json` の parse/update
- plumOS 同梱 SDL2 を使った linked/render test と最小 UI
