# plumOS ユーザー向けガイド

このガイドは、plumOS を Miyoo A30 で使う人向けの入口です。ビルドや実装の詳細ではなく、
SD カードへ導入して、ROM を置き、起動し、設定を変更するために必要な情報をまとめます。

## 最初に読むもの

1. [インストール](install.ja.md)
2. [基本操作](operation.ja.md)
3. [SD カード構成](storage.ja.md)
4. [セーブデータとステート](save-data.ja.md)
5. [対応システム/エミュレータ一覧](emulators.ja.md)

## plumOS の基本方針

- A30 本体の rootfs/NAND は原則変更しません。
- 配布物は SD カード root に展開して使います。
- ROM、BIOS、save/state、スクリーンショット、動画は配布物に含みません。
- plumOS の設定、ログ、runtime は主に `plumos/` 以下へ置きます。
- StockOS の SD card runtime も fallback と互換性のため配布物に含みます。

## 主な機能

- 独自 frontend
- Text / Graphic UI mode
- RetroArch
- PicoArch
- standalone emulator
- Pyxel
- ファイルマネージャー
- 音楽プレイヤー
- ROM サムネイルスクレイピング
- FTP / SFTP / Samba
- USB Disk Mode
- SSH
- emulator 設定の factory reset

## 注意

plumOS は ROM や BIOS を提供しません。各システムで必要な BIOS や game data は、ユーザー自身が
合法的に用意してください。

plumOS として作成したファイルは MIT License です。同梱する第三者 component や
StockOS 由来 file はそれぞれの license が適用されます。詳細は
[Third-party notices](../../THIRD_PARTY_NOTICES.ja.md) を参照してください。
