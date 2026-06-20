# MainUI bootstrap

A30 の stock boot flow は `/mnt/SDCARD/miyoo/app/MainUI` を直接起動します。plumOS
ではこの位置に小さな wrapper を置き、実体を
`/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` へ逃がします。

## 方針

- stock MainUI は `/mnt/SDCARD/miyoo/app/MainUI.stock` として退避する
- `/mnt/SDCARD/miyoo/app/MainUI` は shell wrapper にする
- wrapper は `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` を通常FEとして起動する
- wrapper は stock `/etc/main` が先に起動した `keymon` を止める
- wrapper は `/mnt/SDCARD/plumos/bin/plumos-stock-services boot` で StockOS の
  `MtpDaemon`、`adbd`、`sysntpd` を止め、StockOS が SD card を ASCII filename
  conversion で mount した場合は vfat UTF-8 mount へ正規化する
- wrapper は SD card 正規化後に `plumos-sdcard-cleanup` を background 実行し、
  macOS/Windows 由来の sidecar file (`._*`, `.DS_Store`, `Thumbs.db`, `desktop.ini`,
  `__MACOSX`) を削除する。通常 scope は `Roms`/`roms` と `Images` に限定し、
  FE 起動は cleanup 完了を待たない。全SD scan は `--all` の診断/手動用途に残す
- controller UI は START menu に入る/戻るタイミングでも同じ cleanup を background
  実行する。短時間の連続実行は FE 側で抑制し、ユーザー操作は待たせない
- ROM scanner は sidecar file を ROM/thumbnail 候補から常に除外し、scan 開始時にも
  cache を消さない cleanup を background 起動する。scan 結果の正確さは無視ルールで守り、
  実ファイル削除は非同期で行う
- wrapper は boot 時に `plumos-network-rescue` を自動実行しない
- wrapper は `plumos-network-control --wifi on` を実行するが、既に `wlan0` に IP がある場合は
  wpa_supplicant/udhcpc を停止せず接続を維持し、runtime status とSSHだけを確認する。
  IP が無い場合だけ保存済み Wi-Fi 設定から DHCP/IP取得を試みる
- wrapper は `plumos-network-services start-enabled` で SSH と有効化済み network service を起動する
- `wlan0` に IP が既にある場合だけ `plumos-stock-services network-ready` を呼び、
  plumOS 管理の `ntpd` を起動する
- START menu と Network Settings から Network Recovery へ入る導線は持たない
- 通常FEが Mali renderer の初期化と初回描画に成功したら
  `/tmp/plumos-fe-ready` を作る。wrapper はこの flag がある場合、FE がその後
  終了しても stock MainUI へ fallback しない
- controller UI が初回描画前に無い/異常終了した場合は旧 `plumos-frontend`、
  それも失敗した場合は stock MainUI へ fallback する
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
wrapper 起動時は `plumos-network-rescue` を実行せず、そのまま通常FEを表示します。
Wi-Fi が既に接続済みの場合はその接続を維持し、未接続の場合だけ保存済み Wi-Fi 設定での
接続を試みます。SSH を含む有効化済み network service は background で開始します。
左右キーは実行/戻るに使わず、実行は A、戻る/キャンセルは B に統一します。

SSH が復旧していない状態でのネットワーク復旧は、FE 内の Network Recovery ではなく、
StockOS MainUI から直接起動できる独立した shell script として設計します。

## FE control

FE の通常操作は以下の単一入口を使います。

```sh
./scripts/a30-fe-control.sh status
./scripts/a30-fe-control.sh restart
./scripts/a30-fe-control.sh stop
./scripts/a30-fe-control.sh start
```

`plumos-fe-control stop` は `/tmp/plumos-fe-paused` を作成してから FE と wrapper を止めます。
stock `/etc/main` は wrapper を再実行しますが、wrapper は pause file が存在する間は
FE を起動せず待機します。`start` または `restart` は pause file を削除し、
起動後に `/dev/fb0` owner が plumOS FE 1 process だけであることを確認します。

## 起動スプラッシュ

StockOS の `LOADING` 表示は kernel logo ではなく、`/etc/init.d/boot` が
`/usr/miyoo/res/loading.data.tgz` を展開して `/dev/fb0` へ書き込む userland 表示です。
rootfs は変更せず、plumOS の `MainUI.wrapper` が起動した直後に
`/mnt/SDCARD/plumos/bin/plumos-boot-splash` で `/dev/fb0` へ plumOS splash を描き直します。
helper は描画後すぐ終了するため、直後の SD card UTF-8 remount をブロックしません。
`/mnt/SDCARD/plumos/config/disable-mainui-wrapper` がある場合は splash も表示しません。

リリース標準の splash 画像は `/mnt/SDCARD/plumos/config/frontend/boot-splash.png` に置きます。
ユーザーは同じパスの PNG を差し替えることで任意の splash 画像を使えます。
推奨解像度は横向き `640x480`、PNG 24bit/32bit sRGB です。A30 の framebuffer は
物理的には `480x640` ですが、plumOS UI と同じ横向き論理解像度 `640x480` として扱います。
画像が無い、または PNG として読めない場合は内蔵 splash に fallback します。サイズが
`640x480` 以外の場合は縦横比を維持して画面内に収め、余白は内蔵 splash と同じ背景で埋めます。

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
/mnt/SDCARD/plumos/logs/sdcard-cleanup.log
```
