# plumOS 設計方針

この文書は、Miyoo A30 向け plumOS の構成方針を固定するためのメモです。

## 基本方針

- plumOS の実体は `/mnt/SDCARD/plumos` 配下に集約する
- stock の `/mnt/SDCARD/miyoo/lib` や `/mnt/SDCARD/RetroArch` に依存しない
- フロントエンド、ライブラリ、RetroArch、libretro core、設定、cache、log を
  plumOS 側で管理する
- A30 の rootfs/NAND は初期段階では書き換えない
- 既存ファイルは「互換性調査の入力」として読むが、実行時依存は減らしていく
- 既存仕様は正解として扱わず、計測と検証で置き換え判断をする

## 想定 directory 構成

```text
/mnt/SDCARD/plumos/
  bin/                  frontend, launcher, service commands
  lib/                  plumOS bundled shared libraries
  libexec/              helper programs
  runtime/              dynamic linker/sysroot pieces if needed
  frontend/             frontend assets and modules
  retroarch/
    bin/                RetroArch executable
    cores/              libretro core .so files
    info/               core info files
    assets/             RetroArch assets
    shaders/            shaders if usable on A30
  config/
    retroarch/          retroarch.cfg and overrides
    frontend/           plumOS frontend settings
    systems/            system launch profiles
  state/
    saves/
    states/
    playlists/
    recent/
  cache/
  logs/
  ssh/
```

## 既存起動フローとの境界

A30 の stock 起動処理は `/mnt/SDCARD/miyoo/app/MainUI` を直接起動します。このため、
完全に独立した構成を目指す場合でも、最初の起動口として以下のどちらかが必要です。

- `/mnt/SDCARD/miyoo/app/MainUI` を小さな wrapper に置き換える
- rootfs 側の `/etc/main` 相当を将来的に置き換える

初期段階では安全性を優先し、前者を採用します。`MainUI` wrapper は最小限の処理だけを
行い、実体は `/mnt/SDCARD/plumos/bin/plumos-frontend` を起動します。

wrapper に必要な条件:

- 起動失敗時に stock MainUI へ戻せること
- SD カードだけで復旧できること
- log を `/mnt/SDCARD/plumos/logs` へ残すこと
- plumOS 側の runtime path を明示すること

## ランタイム方針

stock library を流用しないため、plumOS の binary は以下のどちらかで動かします。

- musl などで静的リンクできるものは静的リンクする
- 動的リンクが必要なものは plumOS 側に dynamic linker と shared library を同梱する

RetroArch や SDL など、動的 library 依存が大きいものは、A30 向け sysroot を明示して
build します。A30 の stock glibc は `2.23` なので、汎用 Linux armhf binary に
依存しない前提で進めます。

## RetroArch 方針

- RetroArch は `/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ配置する
- core は `/mnt/SDCARD/plumos/retroarch/cores` へ配置する
- `HOME=/mnt/SDCARD/RetroArch` のような既存方式には依存しない
- `--config` で plumOS 管理の `retroarch.cfg` を明示する
- system ごとの差分は launch profile または override config で管理する
- core 更新は system ごとに段階的に行い、起動、save/state、input、performance を
  確認してから採用する

既存 script 互換が必要な期間は、compatibility launcher で stock の `launch` /
`launchlist` を読めるようにします。ただし最終的には shell script が散らばる構成から、
plumOS 側の launch profile へ寄せます。

## CPU 方針

既存 script は CPU governor を `performance` にしたり、CPU2/CPU3 を offline にする
ことがあります。これは最適化の可能性もありますが、現時点では妥当性未確認です。

plumOS では以下を検証します。

- すべての CPU core を online にした場合の performance
- CPU2/CPU3 を offline にした場合の発熱、battery、stutter
- `performance`, `ondemand`, `interactive` governor の違い
- system/core ごとの必要 clock
- game 終了後に CPU 状態を確実に戻す仕組み

CPU policy は launch script に分散させず、plumOS の launcher が一元管理します。

## Wi-Fi 方針

Wi-Fi は kernel driver、電源投入、`wlan0`、`wpa_supplicant`、DHCP client が関係します。
stock の init script は `devmem` で電源系を操作しているため、最初は既存手順を観察し、
plumOS 側で安全に再現できるか確認します。

目標:

- Wi-Fi 設定 UI は plumOS frontend に持たせる
- 機微情報は `/config` または plumOS の private config に置き、git へ入れない
- userland の `wpa_supplicant` と DHCP client は可能なら plumOS 同梱へ移す
- kernel module や radio power sequence は、A30 固有の制約として扱う

## Input 方針

stock は `keymon` を companion process として起動します。まずは `keymon` を残して
frontend の置き換えを進めます。その後、以下を調査します。

- `/dev/input/event*` から直接 button state を読めるか
- SDL2 game controller mapping で十分か
- RetroArch hotkey と frontend 操作をどう分離するか
- suspend/resume や brightness/volume key の扱い

## 既存仕様を見直す観点

置き換え候補:

- system ごとの shell launch script
- `HOME` と relative path に強く依存する RetroArch 起動
- CPU governor/offline/overclock の分散管理
- stock `miyoo/lib` 依存
- stock `keymon` 依存
- stock Wi-Fi userland 依存

維持すべき互換性:

- 既存 SD カードの `Emu`, `RApp`, `App`, `Roms`, `Imgs`, `Themes` を読めること
- 既存 ROM や artwork を移動せずに認識できること
- 既存の save/state を移行できること
- 起動に失敗しても SD カード操作だけで戻せること

## 直近の実験

1. rollback 可能な `MainUI` wrapper を作る
2. `/mnt/SDCARD/plumos/bin/plumos-env` で runtime path を固定する
3. stock MainUI を残したまま、plumOS frontend prototype を手動起動する
4. SDL/input/audio/video の最小 test binary を A30 上で動かす
5. RetroArch `v1.22.2` を A30 向けに build し、1 system だけで検証する
6. CPU policy の比較 log を取る
