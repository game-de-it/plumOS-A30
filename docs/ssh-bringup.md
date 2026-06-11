# SSH 導入手順

最初のリモート shell には Dropbear を使います。小さく、組み込み機器向けで、
静的リンクの armhf binary として扱いやすいためです。

現在の build は upstream が 2026-05-10 に公開した Dropbear `2026.91` を使います。
固定している tarball の SHA-256 は以下です。

```text
defa924475abf6bc1e74abc00173e46bfdc804bd47caafa14f5a4ef0cc76da34
```

これは開発用 access kit です。A30 の stock rootfs は read-only で、SD カードは
FAT/exFAT 系のため Unix permission を表現できません。そのため、この Dropbear build
では `authorized_keys` の owner/mode check を緩めています。

## ビルド

```sh
./scripts/build-ssh-kit.sh
```

任意: ARM 用の `strip` tool が見つかる場合は自動で binary を strip します。この作業
環境で Docker を使った strip を明示する場合は以下を実行します。

```sh
PLUMOS_DOCKER_STRIP=1 ./scripts/build-ssh-kit.sh
```

## SD カード準備

1. `dist/plumos-a30-ssh-kit/plumos/ssh/etc/authorized_keys` を編集する
2. 作業用 PC の SSH 公開鍵を 1 行で入れる
3. `dist/plumos-a30-ssh-kit/` の中身を SD カード直下へコピーする

コピー後の SD カードには以下が入ります。

```text
/mnt/SDCARD/plumos/ssh/start-ssh.sh
/mnt/SDCARD/plumos/ssh/bin/dropbear
/mnt/SDCARD/plumos/ssh/bin/dropbearkey
/mnt/SDCARD/plumos/ssh/bin/scp
/mnt/SDCARD/plumos/ssh/etc/authorized_keys
/mnt/SDCARD/Roms/PORTS/Start SSH.sh
/mnt/SDCARD/Roms/PORTS/Stop SSH.sh
```

## 起動

Miyoo A30 で Ports を開き、`Start SSH` を実行します。

log は以下へ出力されます。

```text
/mnt/SDCARD/plumos/ssh/log/dropbear.log
/mnt/SDCARD/plumos/ssh/log/network.txt
```

UI や router で IP address が分からない場合は、SD カードを抜いて `network.txt` を
確認します。このファイルには利用可能な network command の出力と `/proc/net` の
情報が入ります。

log に public key が見つからないと出た場合は、SD カード上の
`plumos/ssh/etc/authorized_keys` を実際の作業用 PC の公開鍵で置き換えてください。
comment 行の example key は無視されます。

## 接続

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

default 以外の key を使う場合:

```sh
ssh -i ~/.ssh/YOUR_KEY -p 2222 root@A30_IP_ADDRESS
```

SSH shell と `ssh host command` の PATH は、plumOS runtime を優先するように設定します。

```text
/mnt/SDCARD/plumos/gnu/bin:/mnt/SDCARD/plumos/bin:/usr/miyoo/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

`ssh host command` の PATH は build 時の Dropbear default で決まります。対話ログインの
BusyBox ash は stockOS の `/etc/profile` で PATH を一度 stock 値へ戻すため、Dropbear が
`ENV=/mnt/SDCARD/plumos/ssh/etc/ashrc` を渡し、ash が `/etc/profile` の後に SD カード上の
`ashrc` を読んで PATH を戻します。rootfs の `/etc/profile` は変更しません。

これにより、ログイン直後から `crc32`, `unzip`, `wget`, `plumos-fe-control` などを
full path なしで実行できます。`PLUMOS_SSH_PATH` を指定して `start-ssh.sh` を起動した場合は、
起動 script 側の PATH だけを上書きできます。ただし session 内の default PATH は
Dropbear build と `plumos/ssh/etc/ashrc` に固定されるため、release 用には
`scripts/build-ssh-kit.sh` の `DEFAULT_ROOT_PATH` と `package/ssh-kit/plumos/ssh/etc/ashrc`
を更新して rebuild します。

対話 SSH の command history は `plumos/ssh/etc/ashrc` で `HISTSIZE=9999` に設定し、
`/mnt/SDCARD/plumos/state/ssh/ash_history` に保存します。保存先は
`PLUMOS_SSH_HISTFILE`、件数は `PLUMOS_SSH_HISTSIZE` で上書きできます。履歴には入力した
command がそのまま残るため、SSID、password、token などの機微情報を command line に直接
書かない運用にします。stock `/bin/sh` は履歴を file に保存しない構成のため、対話 SSH では
`ashrc` から `/mnt/SDCARD/plumos/bin/busybox ash -i` へ一度だけ切り替えます。
`ssh host command` のような非対話 command には影響しません。

対話 SSH の TTY へ BusyBox `ls` が名前だけを直接出す場合、UTF-8 の日本語 file name を
`?` に置き換えることがあります。`ashrc` では対話 session の `ls` を shell function とし、
TTY 出力時だけ BusyBox `ls` の stdout を一度 pipe/file 経由にして、UTF-8 file name を
そのまま表示します。`ssh host command` のような非対話 command には影響しません。

## 実機情報の収集

SSH 接続できるようになったら、作業用 PC 側で以下を実行します。

```sh
./scripts/collect-a30-info.sh root@A30_IP_ADDRESS
```

収集結果は timestamp 付きの directory として `artifacts/` 配下に保存されます。
