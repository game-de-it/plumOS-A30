# plumOS ドキュメント索引

この `docs/` は、ユーザー向けの使い方と、開発者向けの技術資料を分けて管理します。
日本語文書を正本とし、英語版がある文書は同名の `.en.md` に置きます。

## ユーザー向け

plumOS を使う人が読む入口です。SD カードへ展開して遊ぶために必要な情報をまとめます。

- [ユーザー向けガイド](user/README.md)
- [インストール](user/install.md)
- [基本操作](user/operation.md)
- [SD カード構成](user/storage.md)
- [対応システム/エミュレータ一覧](user/emulators.md)

## 開発者向け

plumOS をビルド、修正、検証、リリースする人が読む入口です。Docker toolchain と
A30 実機 deploy loop を中心にまとめます。

- [開発者向けガイド](developer/README.md)
- [Docker ビルドガイド](developer/build.md)

## 詳細設計/調査メモ

以下は実装判断や検証履歴を含む詳細資料です。ユーザー向け文書からは必要な範囲だけを
要約し、細かい根拠や再現手順はこちらに残します。

- [plumOS 設計方針](plumos-design-policy.md)
- [設定ファイル配置方針](plumos-config-layout.md)
- [A30 UI design](a30-ui-design.md)
- [A30 設定 UI 方針](a30-settings-policy.md)
- [A30 input policy](a30-input-policy.md)
- [A30 joystickd](a30-joystickd.md)
- [MainUI bootstrap](mainui-bootstrap.md)
- [frontend data model](frontend-data-model.md)
- [frontend theme model](frontend-theme-model.md)
- [network services](network-services.md)
- [USB Disk Mode](usb-disk-mode.md)
- [Docker toolchain 詳細](docker-toolchain.md)
- [runtime package](runtime-package.md)
- [SD root package](sdroot-package.md)
- [release artifacts](release-artifacts.md)
- [emulator runtime manifest](emulator-runtime-manifest.md)
- [FE 実行可能ターゲット一覧](emulator-fe-targets.md)
- [emulator issue triage](emulator-issue-triage.md)
- [libretro core version inventory](libretro-core-version-inventory.md)

## 機械可読な表

これらはドキュメント本文から参照する正本データです。手作業で意味を変えず、生成 script
または検証作業に合わせて更新します。

- [emulator-runtime-manifest.tsv](emulator-runtime-manifest.tsv)
- [emulator-runtime-verification.tsv](emulator-runtime-verification.tsv)
- [emulator-fe-libretro-targets.tsv](emulator-fe-libretro-targets.tsv)
- [emulator-fe-standalone-targets.tsv](emulator-fe-standalone-targets.tsv)
- [libretro-built-cores.tsv](libretro-built-cores.tsv)
- [libretro-core-version-inventory.tsv](libretro-core-version-inventory.tsv)
- [onion-libretro-source-lock.tsv](onion-libretro-source-lock.tsv)
