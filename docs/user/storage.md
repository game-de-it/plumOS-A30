# SD カード構成

plumOS の配布物は SD カード root に直接展開します。ここでは、ユーザーが触ることの多い
directory と、通常は触らない runtime directory を分けて説明します。

## SD カード直下

| Path | 用途 |
| --- | --- |
| `Roms/` | ROM や game data を置く場所 |
| `Bios/` | BIOS や system file を置く場所 |
| `Images/` | plumOS の通常サムネイル置き場 |
| `Imgs/` | StockOS 互換/旧 artwork 置き場 |
| `Saves/` | save data / state の互換置き場 |
| `plumos/` | plumOS runtime、設定、ログ |
| `miyoo/` | StockOS SD payload と plumOS boot wrapper |
| `Emu/` | StockOS 側 emulator payload |
| `RetroArch/` | StockOS 側 RetroArch payload |
| `RApp/` | StockOS/互換用 app payload |
| `App/` | StockOS/互換用 app payload |
| `Themes/` | StockOS theme payload |

## ROM directory

ROM は `Roms/<directory>/` へ置きます。plumOS は Miyoo 系の大文字/省略名と、
EmulationStation 系の小文字名の両方を認識する方針です。

例:

```text
Roms/FC/
Roms/nes/
Roms/SFC/
Roms/snes/
Roms/GBA/
Roms/gba/
```

どの directory 名と拡張子を使えるかは [対応システム/エミュレータ一覧](emulators.md) を参照してください。

## 画像 directory

plumOS の通常サムネイル置き場は `Images/<ROM directory name>/` です。ROM が置かれている
directory 名に合わせて画像 directory を作ります。

例:

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png
```

`Roms/SFC/` に ROM を置いた場合は、画像も `Images/SFC/` に置くのが基本です。

```text
Roms/SFC/Akumajou Densetsu.sfc
Images/SFC/Akumajou Densetsu.png
```

`Imgs/` は StockOS 互換/旧置き場として残しています。新しい plumOS 用サムネイルは
`Images/` を使ってください。

## BIOS directory

BIOS は `Bios/` 以下に置きます。

```text
Bios/
Bios/psx/
Bios/pcenginecd/
```

一部の emulator/core は `Bios/<ROM directory name>/` を見るものと、`Bios/` root を見るものがあります。
必要な BIOS 名と配置は core によって異なります。

## plumOS directory

通常ユーザーが直接編集する必要は少ないですが、問題調査や設定バックアップで重要です。

| Path | 用途 |
| --- | --- |
| `plumos/bin/` | launcher、helper、frontend wrapper |
| `plumos/config/frontend/` | frontend 設定と system 定義 |
| `plumos/config/system/` | 音量、明るさ、言語など |
| `plumos/config/network/` | NW Service など |
| `plumos/config/performance/` | performance 設定 |
| `plumos/config/standalone/` | standalone/PicoArch/Pyxel などの launcher env |
| `plumos/logs/` | plumOS runtime log |
| `plumos/retroarch/` | plumOS 側 RetroArch runtime |
| `plumos/retroarch/cores/` | plumOS 側 libretro core |
| `plumos/state/` | runtime state |
| `plumos/factory-defaults/` | factory reset 用 default |
| `plumos/ssh/` | SSH/SFTP 用 Dropbear |
| `plumos/themes/` | plumOS frontend theme |
| `plumos/python/` | Python runtime |

## ユーザーがバックアップすべきもの

SD カードを作り直す前に、必要に応じて以下をバックアップしてください。

- `Roms/`
- `Bios/`
- `Images/`
- `Imgs/`
- `Saves/`
- `plumos/config/`
- `plumos/state/`
- `plumos/retroarch/saves/`
- `plumos/retroarch/states/`

ROM、BIOS、save/state、個人設定は配布物に含まれません。
