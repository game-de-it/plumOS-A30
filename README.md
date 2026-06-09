# plumOS A30

Miyoo A30 向けカスタムファームウェア `plumOS` の開発ワークスペースです。

このプロジェクトでは、A30 の既存 rootfs をできるだけ触らず、SD カード上の
`/mnt/SDCARD/plumos` 配下にフロントエンド、ライブラリ、RetroArch、libretro
core、設定、ログを集約して動かすことを目標にします。

開発の基本ループは、Docker 内で A30 向けのファイルを build し、SSH 経由で実機へ
転送して動作確認し、log を回収して次の修正へ進む形にします。最終的な配布物には、
SD カードへ展開する runtime package だけでなく、開発者向けの Docker/toolchain
関連ファイルも含めます。

## 現在の状態

最初のマイルストーンとして、A30 へ SSH で入るための Dropbear ベースの開発用
SSH キットを作成しました。SSH 経由で起動処理、マウント状況、Wi-Fi、既存
フロントエンド、RetroArch 構成を調査済みです。

調査結果と設計方針は以下にまとめています。

- [Codex/agent 作業ルール](AGENTS.md)
- [A30 システム調査メモ](docs/a30-system-notes.md)
- [plumOS 設計方針](docs/plumos-design-policy.md)
- [Docker toolchain](docs/docker-toolchain.md)
- [MainUI bootstrap](docs/mainui-bootstrap.md)
- [A30 runtime probe](docs/a30-runtime-probe.md)
- [A30 input policy](docs/a30-input-policy.md)
- [Frontend prototype](docs/frontend-prototype.md)
- [Stock frontend inventory](docs/stock-frontend-inventory.md)
- [plumOS frontend data model](docs/frontend-data-model.md)
- [plumOS frontend theme model](docs/frontend-theme-model.md)
- [A30 設定 UI 方針](docs/a30-settings-policy.md)
- [plumOS ネットワークサービス](docs/network-services.md)
- [plumOS USB Disk Mode](docs/usb-disk-mode.md)
- [Stock reuse decision template](docs/decisions/stock-reuse-template.md)
- [SSH 導入手順](docs/ssh-bringup.md)
- [TODO](TODO.md)

英語版は同名の `.en.md` ファイルとして残します。

## SSH キットのビルド

```sh
./scripts/build-ssh-kit.sh
```

このスクリプトは Dropbear をダウンロードし、固定した SHA-256 を検証したうえで
`arm-linux-musleabihf` 向けに静的リンクのバイナリをビルドします。生成物は以下に
配置されます。

```text
dist/plumos-a30-ssh-kit/
dist/plumos-a30-ssh-kit.tar.gz
```

SD カードへコピーする前に、作業用 PC の公開鍵を次のファイルへ入れてください。

```text
dist/plumos-a30-ssh-kit/plumos/ssh/etc/authorized_keys
```

その後、`dist/plumos-a30-ssh-kit/` の中身を A30 の SD カード直下へコピーします。
A30 側では Ports から `Start SSH` を実行します。

接続例:

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

## ファイル転送サービス

Network Settings から FTP、SFTP、Samba を ON/OFF できます。共有起点はいずれも
`/mnt/SDCARD/` です。実測では、USB cable を使えるなら USB Disk Mode、network 越しの
速度優先なら FTP、安全な標準ツール互換なら SFTP、Windows/macOS のネットワーク
ドライブ用途なら Samba を推奨します。

各サービスの通常推奨は 10 並列転送、入口上限は 20 です。詳細な測定値と運用方針は
[plumOS ネットワークサービス](docs/network-services.md) にまとめます。

FunKey-OS の USB Mass Storage 方式を参考にした `USB Disk Mode` も実験機能として
検証中です。これは `/mnt/SDCARD` を unmount して SD card partition を PC に直接見せる
方式です。Windows/macOS での認識と転送検証は完了しており、正式化前の細部は
[plumOS USB Disk Mode](docs/usb-disk-mode.md) にまとめます。

## 参考資料

- Miyoo A30 toolchain reference: https://codeberg.org/hydrogen18/miyooa30
- Dropbear SSH: https://matt.ucc.asn.au/dropbear/dropbear.html
