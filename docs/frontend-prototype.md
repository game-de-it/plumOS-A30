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
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali.bin
dist/plumos-frontend/plumos/bin/plumos-safe-hotkeyd
dist/plumos-frontend/plumos/bin/plumos-safe-shutdown
dist/plumos-frontend/plumos/config/frontend/systems.json
dist/plumos-frontend/plumos/config/frontend/menus.json
dist/plumos-frontend/plumos/config/frontend/apps.json
dist/plumos-frontend/plumos/config/frontend/themes.json
dist/plumos-frontend/plumos/config/frontend/settings.json
dist/plumos-frontend/plumos/themes/default/theme.json
dist/plumos-frontend/plumos/lib/
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
- canonical thumbnail root `/mnt/SDCARD/Images/<system_id>` の単一路線
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

`menu start` は `menus.json` を読み、settings/apps/favorites/network/shutdown へ
辿る menu model を表示します。A30 の stock MainUI には reboot 項目がないため、
安全な再起動手順が分かるまで `Reboot` は START menu に出しません。
この prototype は action を表示するだけで、shutdown は実行しません。
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

`plumos-controller-ui` は controller-first の最小 prototype です。既定の
`plumos-controller-ui` は SSH stdout に TOP/ROM list/START menu/Favorites/Recent/Settings の
状態を描き、`/dev/input/event*` または stdin fallback から入力を受けます。A30 では
`/proc/bus/input/devices` から `gpio-keys-polled` を探し、通常は `/dev/input/event3` を
自動選択します。
`plumos-input-compare` では、stock `keymon` と stock `MainUI` が動作中でも
`/dev/input/event3` を非排他で直接 open/poll できることを確認しています。
実機 mapping では A=`KEY_SPACE`, B=`KEY_LEFTCTRL`, START=`KEY_ENTER`,
SELECT=`KEY_RIGHTCTRL` を使います。Function=`KEY_ESC` は START 代替ではなく、
SAFE menu に割り当てています。

`plumos-safe-hotkeyd` は、emulator 実行中に frontend がブロックされる間の
安全終了用 helper です。`gpio-keys-polled` を自動検出して `/dev/input/event3`
相当を非排他で読み、既定では Function=`KEY_ESC` の押下で
`plumos-safe-shutdown --shutdown --no-poweroff` を実行します。`SIGUSR1` でも同じ
trigger path を通るため、物理ボタンなしの実機試験に使えます。
2026-06-08 に NES/RetroArch 実行中の `SIGUSR1` 試験で、safe shutdown、resume hold、
CPU復元、FE再起動まで確認しました。artifact は
`artifacts/a30-probes/safe-shutdown/20260608-165456-safe-hotkeyd-sigusr1-nes` です。
`plumos-text-ui launch --execute` は RetroArch/standalone launch 中だけ
`plumos-safe-hotkeyd --oneshot` を自動起動します。`PLUMOS_SAFE_HOTKEYD_AUTOSTART=0`
で無効化できます。自動起動した hotkeyd の `SIGUSR1` trigger は
`artifacts/a30-probes/safe-shutdown/20260608-170909-text-ui-autohotkey-sigusr1-nes`
で確認済みです。
再実行用に `scripts/probe-a30-safe-hotkeyd.sh` を追加しました。`--trigger signal` は
自動試験、`--trigger physical` は実機の Function 押下待ちです。script経由の最新
signal artifact は
`artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes` です。
この artifact では `SAVE_STATE_SLOT 999` と `.state999` 作成、pending resume plan の
`--entry-slot 999` も確認済みです。
物理 Function 押下そのものは
`artifacts/a30-probes/safe-shutdown/20260608-171641-text-ui-autohotkey-physical-nes`
で確認済みで、`safe-hotkeyd.log` に `trigger source=key` が残ります。ゲーム中に
SAFE menu を重ねる方式は継続確認項目です。

`plumos-controller-ui-mali` は同じ controller UI の Mali EGL renderer 付き binary です。
`/dev/fb0` と `/usr/lib/libEGL.so`/`/usr/lib/libGLESv2.so` を使い、stock SDL にはリンクしません。
wrapper は bundled dynamic loader/glibc で `plumos-controller-ui-mali.bin` を起動しますが、
`LD_LIBRARY_PATH` は export しません。これは、UI 内の `plumos-library-scan` 呼び出しで
stock `/bin/sh` が同梱 glibc を誤って読むのを防ぐためです。

A30 の `/dev/fb0` は `480x640` の portrait framebuffer として見えますが、stock MainUI は
raw framebuffer 上では横画面 UI を回転した向きに描いています。`plumos-controller-ui-mali`
は `--rotation auto|none|cw|ccw` を持ち、`auto` では `480x640` framebuffer に対して
stock と同じ raw 向きへ論理 `640x480` UI を描きます。PC で raw capture をそのまま見ると
縦に見える場合がありますが、stock raw capture と同じく 90 度回すと横向きで読める状態が
A30 実画面向けの想定です。

plumOS としての本試験では、stock `MainUI.stock` と `keymon` は起動していない前提にします。
`--stop-mainui --stop-keymon --no-restart-stock` を使うと、stock `/etc/main` supervisor を
一時停止してから `MainUI.stock`/`keymon` を止め、probe 終了時にも stock 側を戻しません。
Wi-Fi/SSH は `wpa_supplicant`/`udhcpc`/`dropbear` が維持しており、MainUI/keymon 停止後も
接続が残ることを確認済みです。比較用に stock 側を戻したい場合だけ `--no-restart-stock`
を外します。

boot wrapper は `plumos-network-rescue` を自動実行せず、通常FE TOPへ進めます。
START menu と Network Settings から Network Recovery へ入る導線は持ちません。
古い `internal:network-recovery` や `--rescue-network` 経路が残っていても、互換画面で
disabled status を出すだけで helper は呼びません。SSH がない状態での復旧は、
StockOS MainUI から直接起動できる独立した shell script として別途設計します。

A30 実機向け UI の文字サイズ、bitmap/FreeType の使い分け、list column alignment の
ルールは [A30 UI design rules](a30-ui-design.md) にまとめます。特に、ユーザーに見える
文字は `1x` 未満にせず、主要項目は `2x` を基準にします。

Mali renderer の実機確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --script down,a,b,q
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 2 --exercise 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 30
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock --no-scan --timeout 5 --exercise 2 --rotation auto
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_ROOT=/mnt/SDCARD/plumos /mnt/SDCARD/plumos/bin/plumos-controller-ui-mali --rescue-network --script a,q --timeout 1 --rotation auto'
```

2026-06-07 の A30 実機確認では、full scan 後に TOP を表示し、
`down,a,b,q` script で TOP から ROM list に入り TOP へ戻る流れまで
`result=frontend_mali_renderer_rc_0` でした。
その後、Mali renderer は A30 向け compact layout に変更しました。長い help 行を
下部の2行ヒントへ分離し、リスト行は空白圧縮と profile 省略で 480px 幅に収めます。
`--exercise 3` で TOP/ROM/Settings/SAFE を自動往復し、stock `MainUI.stock` と
`keymon` が動いたまま 30 秒保持しても `result=frontend_mali_renderer_rc_0` でした。
さらに `--stop-mainui --stop-keymon --no-restart-stock --rotation auto --exercise 2` で、
stock `/etc/main`、`MainUI.stock`、`keymon` が動いていない plumOS 想定状態でも
`result=frontend_mali_renderer_rc_0` でした。終了後も stock 側は復帰させず、
Wi-Fi/SSH は維持され、stale `plumos-controller-ui-mali` process は残っていません。
`/dev/fb0` capture は raw では縦に見えますが、stock MainUI capture と同じく 90 度回転後に
横画面として読めることを確認しました。
当時の `--rescue-network --script a,q` では、Mali UI から A 相当で network rescue helper を呼び、
exit code `0` で終了することを確認しました。2026-06-10 以降、この経路は helper を呼ばない
disabled compatibility screen に変更しています。
Mali renderer はASCIIを組み込みbitmap font、非ASCIIをFreeType fontで描画します。
font は `PLUMOS_MALI_FONT`、theme `font_ui`、A30既存CJK font候補の順で選びます。
UTF-8はbyteではなくcodepoint単位で処理するため、日本語ROM名は `???` になりません。
2026-06-08 に一時 `PLUMOS_ROOT=/tmp/plumos-fonttest` の日本語ROM一覧で
`ドラゴンクエストIII`、`悪魔城伝説`、`ファイナルファンタジー` の表示を
`artifacts/a30-probes/frontend-font-jp-clean2/20260608-215527.visible.cw.png` で確認しました。
ただし `reboot` command は暗転/LED消灯後に Miyoo logo へ進まず、ユーザーが電源長押しで
強制終了してから電源投入しました。当時は A ボタンの network rescue で SSH が復旧しましたが、
現在はこのFE内導線を無効化しています。

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

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script function,q'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --no-clear --script a,function,up,a,q'
```

実機ボタンの raw event を確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-controller-ui --dump-events --timeout 10'
```

操作:

- D-pad up/down: cursor 移動。物理キーは上下のみ software key repeat を持つ
- D-pad right/left: TOP/ROM list/Favorites/Recent では1ページ送り/戻し
- A: TOP では ROM list へ入る。ROM list/Favorites/Recent では launch を実行する
- B: ROM list、Favorites、Recent から TOP へ戻る。Settings/HELP では
  START menu へ戻り、System Settings の `Display Color` / `INFORMATION` サブ項目では
  System Settings へ戻り、Network Settings の `INFORMATION` サブ項目では
  Network Settings へ戻る。START menu では元の画面へ戻る
- START: START menu を開く
- START menu: Settings/Favorites/Recent は実画面へ遷移し、Shutdown は `plumos-safe-shutdown --shutdown --no-poweroff` を実行する。その他は action preview を出す
- 左右キーは決定/実行/戻る/キャンセルには使わない。決定/実行はA、戻る/キャンセルはBで統一する
- SELECT: system/per-ROM core menu
- Function: SAFE menu を開く。SAFE menu は `Sleep`, `Shutdown`, `Cancel` を持つ
- UI Settings: checkbox は A/左右で保存し、選択系は左右で保存する。TOP/ROM の
  表示項目、並び順、ROM scan policy、boot resume は保存後の実挙動へ反映する
- Network Settings: 第一階層は `Wi-Fi`、`Connect Wi-Fi`、`NW Service`、`INFORMATION`。
  `Wi-Fi` の OFF は runtime を停止し、接続開始は `Connect Wi-Fi` で行う。`NW Service` は FTP/SFTP/Samba/USB Disk Mode を扱う。
  Connection/IP/Signal などの情報系は `INFORMATION` サブ項目へ置き、SSID/PSK は表示しない
- SSH stdin fallback: `w/s/a/d`, `e` または space, `b`, `m`, `c`, `f`, `q`

SAFE menu:

- Function はどの画面からでも SAFE menu を開く
- 初期 cursor は誤操作防止のため `Cancel`
- `Sleep` は `plumos-safe-shutdown --sleep --no-poweroff` を実行する
- `Shutdown` は `plumos-safe-shutdown --shutdown --no-poweroff` を実行する
- `Cancel` と B は元の画面へ戻る。LEFT/RIGHT と Function は決定/戻るには使わない
- `plumos-safe-shutdown` 側には power/sleep backend 選択を接続済み。SAFE menu 既定では
  まだ実 poweroff / 実 suspend を発火せず、保存・終了・`sync`・resume hold までを実行する
- emulator 実行中は `plumos-safe-hotkeyd` の直接安全終了pathを先に検証済み。
  物理 Function 押下も確認済み。ゲーム中 overlay menu は未完了

Settings 画面では plumOS frontend 設定と theme 状態に加えて、plumOS-owned な
system 設定も表示します。UI Settings の `Show Empty Systems`、
`Favorites On TOP`、`Recent On TOP`、`Sort Systems`、`Sort ROMs`、`Scan On Enter`、
`Boot Resume Mode` は保存後の controller UI に反映します。System Settings は
`/mnt/SDCARD/plumos/config/system/settings.json` から volume、brightness、lumination、
display color、language、theme 情報を読みます。
System Settings の第一階層は `Volume`、`Brightness`、`Lumination`、`Display Color`、
`Time Settings`、`Language`、`Theme`、`INFORMATION` です。`Volume`、`Brightness`、
`Lumination`、`Language` は左右で plumOS system settings へ保存します。`Display Color`
は A で `Contrast`、`Hue`、`Saturation` のサブ項目へ入り、それぞれ左右で保存します。
`Time Settings` は `Timezone` と `Manual Time` を持ち、timezone は `TZ` 環境と runtime
`/etc/TZ` へ適用します。手動時刻は選択中 timezone のローカル時刻として入力し、
OS 時刻へ反映します。保存は初回 backup、tmp file、fsync、rename、sync を通します。
現在値や backend/policy 情報は `INFORMATION` サブ項目へ集約します。
redacted Wi-Fi runtime status は Network Settings で扱い、
SSID/PSK は読みません。CPU mode は Performance Settings で扱います。
Performance Settings は既存の `plumos-text-ui core system ... --cpu --freq --cores`
経路へ接続済みで、system ごとの `CPU freq` と `CPU Cores` を
`core-overrides.json` に保存します。`CPU freq` は `648/816/1200/1344 MHz` の
固定値だけを見せ、予測しづらい `keep` は削除します。`Reset to Default` で
`systems.json` の `648MHz` / `2 cores` plumOS default に戻します。
Theme は候補名と path の扱いが固まるまで read-only です。Mali renderer では Settings 先頭の `HELP` から操作説明画面へ入ります。
通常画面の下部には常時操作説明を出しません。

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
  7. Shutdown                 system     system:shutdown          yes

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
- `Imgs` 側には stock artwork が入っています。plumOS の通常 thumbnail 置き場は
  `/mnt/SDCARD/Images/<system_id>` に集約し、`Imgs` は必要になった場合の importer 入力として
  扱います。
- `RApp/mednafen_wswan/config.json` の `imgpath` は `../..Imgs/WSC` になっており、
  `../../Imgs/WSC` の typo と思われます。stock 互換では、このような設定ミスも
  検出・補正対象にします。

## 次に足すもの

- ROM file と artwork file の名前対応 rule
- `gamelist` XML の parse
- `recentlist.json` の parse/update
- plumOS 同梱 SDL2 を使った render test と最小 UI
