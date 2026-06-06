# Stock frontend inventory

この文書は、A30 stock frontend の仕様を plumOS へそのまま流用するための仕様書では
ありません。既存 SD カードを理解し、移行や互換 shim が必要か判断するための調査メモです。

plumOS の frontend/launcher 仕様は別途設計します。stock 仕様をそのまま流用する場合は、
必ず採用理由、代替案、影響範囲を整理し、ユーザー確認を取ってから進めます。

## `config.json` の役割

A30 の stock MainUI は、SD カード上の複数の `config.json` を読んで、画面表示、ROM
一覧、artwork、起動 script を決めていると見ています。

主な配置:

- `/mnt/SDCARD/Emu/*/config.json`: 主要 emulator category
- `/mnt/SDCARD/RApp/*/config.json`: 追加/代替 libretro core
- `/mnt/SDCARD/App/*/config.json`: app/tool
- `/mnt/SDCARD/Themes/*/config.json`: theme

代表例:

```json
{
  "label": "GBA",
  "icon": "gba.png",
  "launch": "launch.sh",
  "rompath": "../../Roms/GBA",
  "imgpath": "../../Imgs/GBA",
  "useswap": 0,
  "shortname": 0,
  "hidebios": 0,
  "extlist": "gba|agb|gbz",
  "launchlist": [
    { "name": "GPSP", "launch": "launch.sh" },
    { "name": "MGBA", "launch": "launch1.sh" }
  ]
}
```

## stock MainUI での使われ方

現時点の理解:

- `label`: 画面上の表示名
- `icon`, `iconsel`: menu icon
- `rompath`: ROM directory
- `imgpath`: artwork directory
- `extlist`: ROM list に表示する拡張子 filter
- `launch`: ROM 選択時に呼ぶ shell script
- `launchlist`: 代替 launcher/core の候補
- `gamelist`: arcade 系などの metadata file
- `useswap`, `shortname`, `hidebios`: stock MainUI 側の表示/scan option と推定

重要なのは、`config.json` だけでは実行仕様が完結していない点です。実際の RetroArch
起動、CPU governor、CPU online/offline、overclock、`HOME`、`EMU_DIR` などは各
`launch.sh` に分散しています。

例:

```sh
echo 0 > /sys/devices/system/cpu/cpu3/online
echo 0 > /sys/devices/system/cpu/cpu2/online
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
$EMU_DIR/overclock 816
HOME=/mnt/SDCARD/RetroArch/ retroarch -v -L core.so "$1"
```

一部の `RApp/*/launch.sh` は `EMU_DIR` を自分で定義せずに使っています。このため、
stock MainUI が起動前に `EMU_DIR` を設定している、または別の wrapper 経路がある可能性が
あります。ここはまだ推定です。

## plumOS での扱い

採用しない前提:

- stock `config.json` を plumOS の正式 frontend spec にしない
- stock `launch.sh` 群をそのまま新 launcher 仕様にしない
- CPU policy を shell script に分散させない
- `HOME=/mnt/SDCARD/RetroArch` 方式を新 RetroArch runtime の前提にしない

利用してよい範囲:

- 既存 SD カードを読み取る inventory scanner の入力
- 既存 ROM/artwork/save/state を移行するための compatibility input
- 初期段階の rollback や比較検証用の一時的な互換 shim

plumOS 側で新設計する対象:

- frontend の system/app/theme data model
- ROM/artwork/recent/favorite の管理形式
- RetroArch/core 起動 profile
- CPU/governor/clock policy
- save/state/config の配置

## 判断ゲート

stock 仕様を流用したくなった場合は、実装前に以下を確認します。

- 何を流用するのか
- なぜ plumOS 独自仕様より良いのか
- 将来の RetroArch/core 更新で足かせにならないか
- 既存 SD カード互換だけなら shim で十分ではないか
- ユーザー確認を取ったか

このルールは `config.json`、`launch.sh`、CPU policy、RetroArch 起動方式、theme
構造、recent/favorite 形式のすべてに適用します。

判断記録には [Stock reuse decision template](decisions/stock-reuse-template.md) を使います。
