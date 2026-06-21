# インストール

この手順は、新しくフォーマットした SD カードへ plumOS を導入するためのものです。

## 必要なもの

- Miyoo A30
- microSD カード
- SD カードを読み書きできる PC
- plumOS の SD root package

配布アーカイブ名は以下を想定します。

```text
plumos-sdroot-package.7z
```

## SD カードへ展開

1. SD カードを PC でフォーマットします。
2. `plumos-sdroot-package.7z` を一回だけ解凍します。
3. 解凍された中身を SD カード root に配置します。
4. SD カード直下に `miyoo/`、`plumos/`、`Roms/`、`Bios/` などが見える状態にします。
5. SD カードを A30 に入れて起動します。

正しい配置例:

```text
SDCARD/
  App/
  Bios/
  Emu/
  Images/
  Imgs/
  RApp/
  RetroArch/
  Roms/
  Saves/
  Themes/
  miyoo/
  plumos/
```

間違った配置例:

```text
SDCARD/
  plumos-sdroot-package/
    App/
    Bios/
    miyoo/
    plumos/
```

外側の `plumos-sdroot-package/` directory ができている場合は、その中身を SD カード root へ移動してください。

## ROM と BIOS

ROM は `Roms/` 以下へ置きます。例:

```text
Roms/nes/
Roms/snes/
Roms/gba/
Roms/psx/
```

BIOS は `Bios/` 以下へ置きます。必要な BIOS 名や配置は emulator/core によって異なります。
plumOS の配布物には BIOS は含まれません。

## サムネイル

plumOS の通常サムネイル置き場は `Images/<ROM directory name>/` です。

例:

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png
```

`Imgs/` は StockOS 互換/旧置き場として残しています。新しく作る画像は `Images/` を使ってください。

## SSH

Network Settings の `NW Service` で SSH を ON にすると、PC から接続できます。

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

初期パスワード:

```text
plumos
```

SFTP も SSH と同じパスワードで利用できます。`authorized_keys` を置いた場合は鍵認証も使えますが、
公開配布アーカイブには個人の公開鍵を含めません。

## 既存 SD カードへ上書きする場合

この SD root package は fresh/formatted SD card 向けです。既存 SD カードへ上書きする場合は、
事前に ROM、BIOS、save/state、スクリーンショット、動画、個人設定をバックアップしてください。

既存環境を安全に更新する runtime installer 方式もありますが、通常ユーザー向けの最初の導入では
fresh SD card への展開を推奨します。
