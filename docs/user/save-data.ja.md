# セーブデータとステート

plumOS は、すべての emulator を 1 つの共通 save directory に固定していません。
RetroArch、PicoArch、standalone emulator、Pyxel で runtime home や設定ファイルが異なります。
SD カードを作り直す前に、以下の path をバックアップしてください。

game 内 save と save state は別物です。

- game 内 save は SRAM、メモリーカード、battery save、RPG の save file など、game や
  emulator core が作る通常の save data です。
- save state は emulator の snapshot です。同じ emulator/core 系でしか使えないことが多く、
  emulator を大きく変更すると使えなくなる場合があります。

## まずバックアップすべきもの

ユーザーデータを広めに守る場合は、SD カードから以下をコピーしてください。

- `Roms/`
- `Bios/`
- `Images/`
- `Imgs/`
- `Saves/` 自分で配置した、または StockOS などから持ち込んだ data がある場合
- `plumos/config/`
- `plumos/state/`
- `plumos/retroarch/home/.config/retroarch/`

`Saves/` は互換用、またはユーザーが手動で使うための directory として残しています。
plumOS のすべての emulator が使う統一 save directory ではありません。

## RetroArch

frontend から起動する RetroArch は以下を使います。

- launcher: `plumos/bin/plumos-retroarch-launch`
- runtime wrapper: `plumos/bin/plumos-retroarch-practical`
- main config: `plumos/retroarch/config/retroarch-practical.cfg`
- RetroArch home: `plumos/retroarch/home/`

現在の practical RetroArch 設定では、主に以下が設定されています。

| 設定 | 値 |
| --- | --- |
| `savefile_directory` | `~/.config/retroarch/saves` |
| `savestate_directory` | `~/.config/retroarch/states` |
| `savefiles_in_content_dir` | `true` |
| `savestates_in_content_dir` | `true` |
| `savestate_auto_save` | `true` |
| `savestate_auto_load` | `false` |
| `savestate_auto_index` | `true` |
| `screenshot_directory` | `/mnt/SDCARD/Images` |

`savefiles_in_content_dir` と `savestates_in_content_dir` が有効なので、通常の
RetroArch save file / state は ROM のある content directory の近く、またはその配下に
書かれる想定です。正確な file 名や subdirectory は core や RetroArch 設定によって変わります。

fallback となる基本 directory は以下です。

```text
plumos/retroarch/home/.config/retroarch/saves/
plumos/retroarch/home/.config/retroarch/states/
```

screenshot は `Images/` に保存されます。これは画像/ユーザーメディアであり、game save data
ではありません。

RetroArch menu から directory 設定を変更した場合は、RetroArch 側に表示されている path を
正として、その path もバックアップ対象にしてください。

## PicoArch

frontend から起動する PicoArch は以下を使います。

- launcher: `plumos/bin/plumos-picoarch-launch`
- runtime state root: `plumos/state/picoarch/`

launcher は PicoArch の環境変数を以下のように設定します。

```text
HOME=plumos/state/picoarch
XDG_CONFIG_HOME=plumos/state/picoarch/xdg-config
XDG_DATA_HOME=plumos/state/picoarch/xdg-data
XDG_CACHE_HOME=plumos/state/picoarch/xdg-cache
```

core/ROM ごとの PicoArch 設定は、例えば以下のような directory に作られます。

```text
plumos/state/picoarch/.picoarch-<core>-<rom-tag>/picoarch.cfg
```

PicoArch の save data、save state、core option、cache は基本的に
`plumos/state/picoarch/` 配下の runtime state として扱ってください。一部の libretro core は
ROM の近くに content 固有 file を作る場合もあるため、PicoArch を使う場合も `Roms/` は
バックアップ対象に含めてください。

PicoArch の BIOS 参照先は別設定です。plumOS の既定は `Bios/` で、launcher が core ごとの
PicoArch config に `bios_dir` を書き込む場合があります。

## Standalone

standalone emulator は以下を使います。

- launcher: `plumos/bin/plumos-standalone-launch`
- runtime state root: `plumos/state/standalone/<emulator-id>/`

それぞれの standalone emulator では、plumOS が以下の環境変数を設定します。

```text
HOME=plumos/state/standalone/<emulator-id>
XDG_CONFIG_HOME=plumos/state/standalone/<emulator-id>/config
XDG_DATA_HOME=plumos/state/standalone/<emulator-id>/data
```

主な standalone runtime の置き場は以下です。

| Runtime | 主な save/config 置き場 |
| --- | --- |
| PPSSPP | `plumos/state/standalone/ppsspp/`。config は `config/ppsspp/PSP/SYSTEM/`。PPSSPP の save data / state はこの PPSSPP home tree 配下に置かれます。 |
| PCSX-ReARMed standalone | `plumos/state/standalone/pcsx_rearmed/` |
| ScummVM standalone | `plumos/state/standalone/scummvm/`。plumOS は特に指定がない場合 `config/scummvm.ini` を渡します。 |
| EasyRPG standalone | `plumos/state/standalone/easyrpg/`。RPG Maker の save は project 側に作られることもあるため、`Roms/` 以下の game project もバックアップしてください。 |
| OpenBOR standalone | `plumos/state/standalone/openbor/Saves/`。log や screenshot も `plumos/state/standalone/openbor/` 配下に置かれます。 |

standalone emulator のアプリ内設定で save path を変更した場合は、その emulator が表示する
path を優先し、そこもバックアップしてください。

## Pyxel

Pyxel は plumOS の Pyxel wrapper 経由で起動します。既定では wrapper が以下を設定します。

```text
HOME=/mnt/SDCARD
```

そのため、Pyxel app は app 側が別 path を選ばない限り、SD カード root 配下にユーザーデータを
作成できます。`PLUMOS_PYXEL_HOME` を変更した場合は、その path を優先してください。Pyxel app
directory と、app が作成した data file をバックアップ対象にしてください。

## Factory Reset

emulator の factory reset は、plumOS が用意した emulator 設定 default を復元する機能です。
ROM、BIOS、screenshot、game save を削除する目的の機能ではありません。ただし emulator 設定の
一部は runtime state の近くにあるため、path や操作設定を手動変更している場合は、factory reset
前にバックアップしてください。
