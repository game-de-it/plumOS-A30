# plumOS ネットワークサービス

この文書は、plumOS が提供するファイル転送向けネットワークサービスの初期仕様です。

## 基本方針

- すべてのサービスの共有起点は `/mnt/SDCARD/` とする。
- plumOS 側の実行ファイル、設定、ログ、永続状態は `/mnt/SDCARD/plumos/` 配下に置く。
- stockOS の rootfs や `/config/system.json` は変更しない。
- Network Settings の `FTP`, `SFTP`, `Samba` は checkbox として表示し、A ボタンで ON/OFF する。
- ON は service の start + enable、OFF は stop + disable と同じ扱いにし、再起動後も状態を維持する。

## サービス

### FTP

- 実装: BusyBox `tcpsvd` + `ftpd`
- Port: `21`
- 共有起点: `/mnt/SDCARD/`
- 認証: 初期実装では anonymous/write-enabled

軽量で依存が少ないため、複数ファイル転送の実測で問題がなければ最も手軽な転送手段候補にします。

### SFTP

- 実装: Dropbear SSH + OpenSSH `sftp-server`
- Port: SSH と同じ `2222`
- 共有起点: SFTP server 起動時に `/mnt/SDCARD/` へ移動
- 認証: 既存の SSH 認証を使う

SFTP は SSH daemon にぶら下がるため、OFF にしても SSH 自体は止めません。OFF では
`/mnt/SDCARD/plumos/ssh/libexec/sftp-server` の入口を無効化し、SSH shell は維持します。

### Samba

- 実装: Samba `smbd`
- Port: `445`
- 共有名: `SDCARD`
- 共有起点: `/mnt/SDCARD/`
- 接続例: `smb://A30_IP/SDCARD`
- 認証: `plumos` / `plumos`

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

## 転送速度の検証方針

推奨サービスは実測後に README へ反映します。測定では次の観点を揃えます。

- 単一大容量ファイルの upload/download
- 多数の小さいファイルの upload/download
- 複数ファイル同時転送時の安定性
- macOS Finder / Windows Explorer / FTP client / SFTP client での扱いやすさ
- 転送中に FE 操作や emulator 起動へ悪影響が出ないか

初期候補は、手軽さは Samba、軽量さは FTP、安全な標準ツール互換は SFTP として比較します。
