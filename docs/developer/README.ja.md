# plumOS 開発者向けガイド

このガイドは、plumOS をビルド、変更、検証、リリースする開発者向けの入口です。

## まず読むもの

1. [Docker ビルドガイド](build.ja.md)
2. [機能逆引き](feature-index.ja.md)
3. [Miyoo Mini Flip 移植メモ](porting-miyoo-mini-flip.ja.md)
4. [plumOS 設計方針](../plumos-design-policy.ja.md)
5. [設定ファイル配置方針](../plumos-config-layout.ja.md)
6. [runtime package](../runtime-package.ja.md)
7. [SD root package](../sdroot-package.ja.md)
8. [release artifacts](../release-artifacts.ja.md)
9. [Third-Party Notices](../../THIRD_PARTY_NOTICES.ja.md)

## 技術構成

plumOS は A30 本体の rootfs/NAND を原則書き換えず、SD カード上で成立する構成を目指します。

```text
/mnt/SDCARD/
  miyoo/                    StockOS SD payload + plumOS MainUI wrapper
  plumos/
    bin/                    frontend/launcher/helper
    config/                 plumOS-owned persistent config
    lib/                    bundled runtime libraries
    retroarch/              RetroArch runtime and libretro cores
    emulators/              standalone emulator binaries
    apps/                   Apps menu applications
    python/                 Python/Pyxel runtime
    ssh/                    Dropbear SSH/SFTP runtime
    state/                  runtime state
    logs/                   logs
```

## 主要 component

| Component | 役割 |
| --- | --- |
| frontend | TOP/ROM list/START/Settings/Power/Apps UI |
| bootstrap | StockOS boot flow から plumOS FE を起動する MainUI wrapper |
| userland | BusyBox、rsync、ip、ss、strace など |
| network-services | SSH/FTP/SFTP/Samba service 管理 |
| joystickd | A30 入力を emulator 向け gamepad へ変換 |
| retroarch-practical | 通常利用する RetroArch runtime |
| libretro-cores | RetroArch/PicoArch で使う libretro core |
| picoarch | 軽量 libretro frontend |
| standalone-emulators | PPSSPP、ScummVM、EasyRPG、OpenBOR、PCSX-ReARMed など |
| python-runtime | Python 3 / pip / Pyxel 用 runtime |
| pyxel-a30 | Pyxel launcher と A30 向け実行経路 |
| nextcommander | ファイルマネージャー |
| music-player | plumOS native music player |

## 重要な設計ルール

- plumOS の永続設定は `/mnt/SDCARD/plumos/config/` 以下に置きます。
- stockOS-owned な `/config/system.json` は通常設定として読み書きしません。
- FE 起動/停止/再起動は `scripts/a30-fe-control.sh` を使います。
- A30 実機 command は `scripts/run-a30.sh` を使います。
- A30 への deploy は `scripts/deploy-a30.sh` を使います。
- `/dev/fb0` owner は 1 process だけにします。
- ROM、BIOS、個人情報、ネットワーク秘密情報は git/log/UI に出しません。
- end-user binary package には `LICENSE` と `THIRD_PARTY_NOTICES.md` を含めます。
- binary SD-root release と一緒に developer source/toolchain package を公開します。

## 既存の詳細資料

実装の背景や調査ログは以下にあります。

- [A30 system notes](../a30-system-notes.ja.md)
- [機能逆引き](feature-index.ja.md)
- [A30 input policy](../a30-input-policy.ja.md)
- [A30 joystickd](../a30-joystickd.ja.md)
- [A30 stock SDL video](../a30-stock-sdl-video.ja.md)
- [Miyoo Mini Flip 移植メモ](porting-miyoo-mini-flip.ja.md)
- [MainUI bootstrap](../mainui-bootstrap.ja.md)
- [frontend prototype](../frontend-prototype.ja.md)
- [frontend data model](../frontend-data-model.ja.md)
- [A30 UI design](../a30-ui-design.ja.md)
- [network services](../network-services.ja.md)
- [USB Disk Mode](../usb-disk-mode.ja.md)
- [Docker toolchain 詳細](../docker-toolchain.ja.md)
- [emulator runtime manifest](../emulator-runtime-manifest.ja.md)
- [emulator issue triage](../emulator-issue-triage.ja.md)
- [libretro core version inventory](../libretro-core-version-inventory.ja.md)
