# Frontend prototype

最初の plumOS frontend は、画面描画を始める前の互換性スキャナです。A30 上で stock
SD カード構成を読み、`Emu`, `RApp`, `App`, `Themes` の `config.json` と、そこから
参照される ROM/artwork/metadata の存在を確認します。

## build

```sh
./scripts/docker-build.sh frontend
```

生成物:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
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
- SDL/input を使った最小UI
