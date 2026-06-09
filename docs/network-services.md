# plumOS ネットワークサービス

この文書は、plumOS が提供するファイル転送向けネットワークサービスの初期仕様です。

## 基本方針

- すべてのサービスの共有起点は `/mnt/SDCARD/` とする。
- plumOS 側の実行ファイル、設定、ログ、永続状態は `/mnt/SDCARD/plumos/` 配下に置く。
- stockOS の rootfs や `/config/system.json` は変更しない。
- Network Settings の `FTP`, `SFTP`, `Samba` は checkbox として表示し、A ボタンで ON/OFF する。
- ON は service の start + enable、OFF は stop + disable と同じ扱いにし、再起動後も状態を維持する。
- 小さい ROM ファイルをまとめて転送しやすいよう、各サービスの同時転送入口は 20 を目標にする。
  通常の推奨並列数は 10 とし、20 は余裕枠として扱う。

## サービス

### FTP

- 実装: BusyBox `tcpsvd` + `ftpd`
- Port: `21`
- 共有起点: `/mnt/SDCARD/`
- 認証: 初期実装では anonymous/write-enabled
- 同時接続上限: `20`

軽量で依存が少ないため、複数ファイル転送の実測で問題がなければ最も手軽な転送手段候補にします。

### SFTP

- 実装: Dropbear SSH + OpenSSH `sftp-server`
- Port: SSH と同じ `2222`
- 共有起点: SFTP server 起動時に `/mnt/SDCARD/` へ移動
- 認証: 既存の SSH 認証を使う
- 同時接続入口: Dropbear の `MAX_UNAUTH_PER_IP=20`

SFTP は SSH daemon にぶら下がるため、OFF にしても SSH 自体は止めません。OFF では
`/mnt/SDCARD/plumos/ssh/libexec/sftp-server` の入口を無効化し、SSH shell は維持します。

### Samba

- 実装: Samba `smbd`
- Port: `445`
- 共有名: `SDCARD`
- 共有起点: `/mnt/SDCARD/`
- 接続例: `smb://A30_IP/SDCARD`
- 認証: `plumos` / `plumos`
- 同時接続上限: `20`

Windows/macOS からネットワークドライブとして扱いやすいことを優先します。初期実装では
Samba 内部で `plumos` を `root` へ map し、共有の実効ユーザーも `root` に固定します。A30 では
NetBIOS 探索は使わず、`\\A30_IP\SDCARD` または `smb://A30_IP/SDCARD` で直接マウントします。

## 永続状態

有効/無効状態は次のファイルで管理します。

```text
/mnt/SDCARD/plumos/config/network/services.conf
```

キーは次の通りです。

```text
ftp_enabled=0|1
sftp_enabled=0|1
samba_enabled=0|1
```

boot wrapper と network recovery は `plumos-network-services start-enabled` を呼び、enabled なサービスだけを復帰します。

## 操作

実体は次の helper です。

```sh
/mnt/SDCARD/plumos/bin/plumos-network-services status ftp
/mnt/SDCARD/plumos/bin/plumos-network-services start ftp
/mnt/SDCARD/plumos/bin/plumos-network-services stop ftp
/mnt/SDCARD/plumos/bin/plumos-network-services start-enabled
```

`SERVICE` は `ftp`, `sftp`, `samba`, `all` を受け付けます。

## ログ

```text
/mnt/SDCARD/plumos/logs/network-services.log
/mnt/SDCARD/plumos/logs/samba-*.log
```

SSID、PSK、個人ファイル名をログや git に残さない方針は Network Settings と同じです。

## 転送速度の検証結果

2026-06-10 時点の macOS -> A30 upload 実測は次の通りです。測定は 1MiB ファイル
50 個を 10 並列で転送しました。

| Method | Result | Time | Throughput | Notes |
| --- | ---: | ---: | ---: | --- |
| USB Disk Mode | 50/50 成功 | 5.827 秒 | 8.58 MiB/s | macOS mounted `/Volumes/PLUMOS-A30`; `sync` 後に計測終了。SHA-256一致。 |
| FTP | 50/50 成功 | 8.645 秒 | 5.78 MiB/s | 速度優先の network service 候補。 |
| SFTP | 50/50 成功 | 16.936 秒 | 2.95 MiB/s | Dropbear は `MAX_UNAUTH_PER_IP=20` にする。 |
| Samba | 50/50 成功 | 29.658 秒 | 1.69 MiB/s | macOS では xattr エラーを避けるため `cp -X` を使う。 |

SFTP は 1MiB ファイル 20 個の 20 並列 smoke でも 20/20 成功しています。小さい ROM
をまとめて送る通常推奨は 10 並列、上限入口は 20 とします。

現時点の推奨は、USB cable を使えるなら USB Disk Mode、network 越しの速度優先なら FTP、
標準の安全なツール互換を優先するなら SFTP、Windows/macOS のネットワークドライブとして
扱うなら Samba です。転送中の FE 操作や emulator 起動への影響は、今後の長時間運用で
追加確認します。

USB Disk Mode は network service ではありません。詳細は
[USB Disk Mode](usb-disk-mode.md) にまとめます。
