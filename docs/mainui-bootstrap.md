# MainUI bootstrap

A30 の stock boot flow は `/mnt/SDCARD/miyoo/app/MainUI` を直接起動します。plumOS
ではこの位置に小さな wrapper を置き、実体を
`/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` へ逃がします。

## 方針

- stock MainUI は `/mnt/SDCARD/miyoo/app/MainUI.stock` として退避する
- `/mnt/SDCARD/miyoo/app/MainUI` は shell wrapper にする
- wrapper は `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` を通常FEとして起動する
- wrapper は boot 復旧用に `/mnt/SDCARD/plumos/bin/plumos-network-rescue` を自動実行する
- wrapper は stock `/etc/main` が先に起動した `keymon` を止める
- wrapper は `/mnt/SDCARD/plumos/bin/plumos-stock-services boot` で StockOS の
  `MtpDaemon`、`adbd`、`sysntpd` を止める
- `plumos-network-rescue` は DHCP 後に `plumos-stock-services network-ready` を呼び、
  plumOS 管理の `ntpd` を起動する
- START menu の `Network Recovery` から Network Recovery 画面へ入り、A ボタンで
  Wi-Fi 起動処理、DHCP、SSH start を再実行できる
- controller UI が無い/異常終了した場合は旧 `plumos-frontend`、それも失敗した場合は
  stock MainUI へ fallback する
- log は `/mnt/SDCARD/plumos/logs/` へ出す
- SD カード上のファイル操作だけで rollback できるようにする

## package build

```sh
./scripts/build-bootstrap-package.sh
```

生成物:

```text
dist/plumos-bootstrap/
dist/plumos-bootstrap.tar.gz
```

## install

A30 の SSH が起動している状態で実行します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/install-mainui-wrapper-a30.sh
```

install script は以下を行います。

- `dist/plumos-bootstrap` を `/mnt/SDCARD` へ転送する
- `/mnt/SDCARD/miyoo/app/MainUI.stock` がなければ作成する
- `/mnt/SDCARD/miyoo/app/MainUI` を wrapper に置き換える

bootstrap package は controller UI 本体を含みません。frontend は
`./scripts/docker-build.sh frontend` で別 package として build/deploy します。
wrapper 起動時は `plumos-network-rescue` を background で自動実行し、そのまま通常FEを
表示します。Network Recovery 画面は通常FEの START menu から開き、A ボタンで
`/mnt/SDCARD/plumos/bin/plumos-network-rescue` を再実行します。左右キーは実行/戻るに
使わず、実行は A、戻る/キャンセルは B に統一します。

## disable

wrapper を残したまま stock MainUI を起動したい場合は、A30 上で以下を作ります。

```sh
touch /mnt/SDCARD/plumos/config/disable-mainui-wrapper
```

解除:

```sh
rm -f /mnt/SDCARD/plumos/config/disable-mainui-wrapper
```

## restore

stock MainUI へ戻します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/restore-mainui-wrapper-a30.sh
```

SD カードを直接編集する場合は、以下でも戻せます。

```sh
cp /mnt/SDCARD/miyoo/app/MainUI.stock /mnt/SDCARD/miyoo/app/MainUI
```

## logs

```text
/mnt/SDCARD/plumos/logs/mainui-wrapper.log
/mnt/SDCARD/plumos/logs/plumos-controller-ui-mali.log
/mnt/SDCARD/plumos/logs/plumos-frontend.log
/mnt/SDCARD/plumos/logs/stock-services.log
```
