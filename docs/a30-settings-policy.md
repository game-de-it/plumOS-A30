# A30 設定 UI 方針

この文書は plumOS frontend で扱う A30 固有設定の初期方針です。

目的は、stock frontend の仕様を無条件に流用することではありません。まず A30 の
実機状態を安全に観測し、どの backend を使うべきか検証したうえで、plumOS 側の
設定 UI と service に接続します。

## 基本方針

- plumOS の永続設定は `/mnt/SDCARD/plumos/` 配下に置く。
- 設定ファイル配置の全体方針は `docs/plumos-config-layout.md` に従う。
- stockOS-owned な `/config/system.json` は plumOS の通常設定として参照・書き込みしない。
- A30 本体設定の write-enabled UI は、backup、atomic write、`sync`、復旧方針を
  持つ項目だけに限定する。
- stock 側の仕様を採用する場合は、事前に理由とリスクを確認する。
- Wi-Fi の PSK、`/config/wpa_supplicant.conf` の内容は git と log に出さない。
  PSK は `Connect Wi-Fi` のパスワード入力中だけ確認用に表示し、接続処理や log へ
  渡す際は一時 file 経由にする。SSID は `Connect Wi-Fi` のユーザ選択画面でのみ
  表示し、log/git には残さない。

## controller UI の現在値表示

`plumos-controller-ui` の START menu から System Settings を開くと、ユーザーが
調整対象として認識する次の項目を表示します。2026-06-09 時点では
`/mnt/SDCARD/plumos/config/system/settings.json` を読み書きし、stockOS の
`/config/system.json` には触れません。`Language` 以外は、plumOS 設定保存後に
A30 runtime backend へ即時反映します。

- `Volume`: `volume`。左右または物理音量ボタンで `0..20` を変更し、ALSA `Soft Volume Master` へ反映する
- `Brightness`: `brightness`。左右で `1..20` を変更し、下記の表で `/sys/devices/virtual/disp/disp/attr/lcdbl` の RAW 値へ反映する。将来は brightness hotkey と連動する
- `Lumination`: `lumination`。左右で `0..10` を変更し、display `enhance` へ反映する
- `Display Color`: A でサブ項目を開き、`Contrast`, `Hue`, `Saturation` をそれぞれ `0..20` で変更する
- `Time Settings`: A で時刻設定サブ項目を開く。`Timezone` は左右で変更し、plumOS config 保存後に `TZ` 環境と runtime `/etc/TZ` へ反映する。`Manual Time` は専用画面で Year/Month/Day/Hour/Minute を変更し、A で OS 時刻へ適用する
- `Language`: `language`。左右で `English`, `Japanese`, `Chinese`, `Traditional Chinese`, `Korean`, `Spanish`, `Portuguese` を選択する
- `INFORMATION`: 現在値や backend/policy などの情報系サブ項目

`INFORMATION` サブ項目には以下の read-only 情報を置きます。

- `Device Model`: 固定値 `Miyoo A30`
- `Linux Kernel`: `/proc/sys/kernel/osrelease`
- `SD Card`: `statvfs()` で見た空き容量/総容量
- `plumOS System Config`: `/mnt/SDCARD/plumos/config/system/settings.json` の読み取り状態
- `Input Device`: `gpio-keys-polled` から見つけた `/dev/input/event*`
- `Audio Backend`: 検出した audio backend。A30 では ALSA `Soft Volume Master` を使う
- `Display Backend`: 検出した display backend。A30 では `disp attr lcdbl/enhance`
- `Write Policy`: stockOS から切り離し、plumOS 配下だけへ保存する方針

`Time Settings` の永続値は `/mnt/SDCARD/plumos/config/system/settings.json` の
`timezone` を原本にします。これは stockOS `/config/system.json` ではありません。
ただしユーザーが期待する「OS のタイムゾーン変更」にするため、保存時、FE 起動時、
MainUI wrapper 起動時に `TZ` 環境と runtime `/etc/TZ` へ適用します。手動時刻設定は
選択中 timezone のローカル時刻として入力し、UTC epoch に変換して `settimeofday()` で
OS 時刻へ反映します。NTP が直後に上書きしないよう、手動適用時は plumOS 管理 NTP を
そのセッションで停止します。

Network Settings の第一階層では次の操作項目だけを扱います。

- `Wi-Fi`: checkbox。OFF は `plumos-network-control --wifi off` を呼び、保存済み認証情報は編集しない。
  ON 側は Network Recovery を呼ばず、接続開始は `Connect Wi-Fi` から行う。
- `Connect Wi-Fi`: A で SSID 検索、パスワード入力、DHCP、default gateway ping、
  IP 表示までの接続フローを開く。PSK は一時 file 経由で backend へ渡し、log へ出さない。
- `NW Service`: SSH/FTP/SFTP/Samba/USB Disk Mode をまとめた network service サブ項目を開く。
- `INFORMATION`: read-only の network 情報サブ項目を開く。

`NW Service` サブ項目には次の操作を置きます。

- `SSH`: checkbox。A で Dropbear SSH shell を ON/OFF する。Port は `2222`。未設定時は従来互換で ON 扱いにする。
  OFF にすると SSH daemon に依存する SFTP も OFF になる。
- `FTP`: checkbox。A で BusyBox FTP service を ON/OFF する。共有起点は `/mnt/SDCARD/`。
- `SFTP`: checkbox。A で Dropbear SFTP subsystem を ON/OFF する。OFF にしても SSH shell は止めない。
  ON にした場合は SFTP が SSH daemon に依存するため SSH も有効化する。
- `Samba`: checkbox。A で `SDCARD` SMB share を ON/OFF する。Windows/macOS で認証を求められた場合は `plumos` / `plumos` を使う。
- `USB Disk Mode`: A で確認画面を開き、A 再押下で SD card partition を USB Mass Storage として PC へ渡す。

`INFORMATION` サブ項目には次の情報を置きます。

- `Connection`: `/tmp/wpa_status.txt` の `wpa_state`
- `IP Address`: `/tmp/wpa_status.txt` の `ip_address`
- `Signal`: `/tmp/wpa_status.txt` の `RSSI`
- `Link Speed`: `/tmp/wpa_status.txt` の `LINKSPEED`
- `Frequency`: `/tmp/wpa_status.txt` の `FREQUENCY`
- `SSH`: plumOS の remote access path。現状は Dropbear port 2222
- `FTP`: `plumos-network-services status ftp`
- `SFTP`: `plumos-network-services status sftp`
- `Samba`: `plumos-network-services status samba`

Wi-Fi 状態、IP、信号などの情報行で A を押しても recovery は実行しません。
Performance Settings では launcher/core profile 由来の CPU 方針を扱います。
2026-06-09 時点では system を選び、`CPU freq` と `CPU Cores` を左右で変更できます。
`CPU freq` は `648/816/1200/1344 MHz` の固定値だけを見せ、予測しづらい `keep` は
削除します。保存は既存の `plumos-text-ui core system ... --cpu --freq --cores` 経路を使い、
`/mnt/SDCARD/plumos/state/frontend/core-overrides.json` に system override として入ります。
`Reset to Default` は `systems.json` の `648MHz` / `2 cores` plumOS default へ戻します。
controller UI 起動時も FE runtime の基準として `userspace 648MHz` / `2 cores` を設定します。

Wi-Fi runtime は `/tmp/wpa_status.txt` から必要な key だけを読みます。SSID や PSK は
読みません。

## brightness

観測した A30 では `/sys/class/backlight` に利用できる brightness file がありません。
plumOS 側では `/mnt/SDCARD/plumos/config/system/settings.json` に `brightness`,
`lumination`, `contrast`, `hue`, `saturation` を持ちます。

UI からは plumOS 側の `brightness`, `lumination`, `contrast`, `hue`,
`saturation` を backup 付き atomic write で更新し、保存直後に A30 の
`/sys/devices/virtual/disp/disp/attr/lcdbl` と
`/sys/devices/virtual/disp/disp/attr/enhance` へ反映します。ユーザー向けの
`brightness 1..20` は、A30 の見た目の変化に合わせて次の RAW 値へ割り当てます。

| brightness | lcdbl RAW |
|---:|---:|
| 1 | 2 |
| 2 | 3 |
| 3 | 4 |
| 4 | 5 |
| 5 | 6 |
| 6 | 7 |
| 7 | 8 |
| 8 | 9 |
| 9 | 10 |
| 10 | 26 |
| 11 | 43 |
| 12 | 59 |
| 13 | 75 |
| 14 | 92 |
| 15 | 108 |
| 16 | 125 |
| 17 | 141 |
| 18 | 157 |
| 19 | 174 |
| 20 | 190 |

`lumination 0..10` は `enhance` の第2値 `0..50`、
`contrast` / `hue` / `saturation 0..20` は `enhance` の第3-5値 `0..100` に丸めます。

## volume

plumOS 側では `/mnt/SDCARD/plumos/config/system/settings.json` に `volume` を持ちます。
UI からは plumOS 側の `volume` を backup 付き atomic write で更新します。
`plumos-volume-control` はこの値を読み、`0..20` を ALSA softvol の `0..255` へ丸めて
`Soft Volume Master` に反映します。`Soft Volume Master` は ALSA default PCM を一度
open するまで見えないため、FE と launcher は必要に応じて短い無音再生で初期化します。
RetroArch と standalone emulator の plumOS launcher は ALSA `default` を既定にし、
同じ softvol を通します。OSS を明示した RetroArch 起動は互換用 fallback とし、その場合だけ
RetroArch の software volume に保存値を反映します。
物理音量ボタンは FE 表示中は controller UI が、RetroArch 実行中は
`plumos-safe-hotkeyd --oneshot` が、standalone emulator 実行中は
`plumos-safe-hotkeyd --volume-only` が `plumos-volume-control up|down` 相当の処理を呼び、
同じ `volume` 設定と softvol に反映します。

## Wi-Fi

runtime 状態は `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, `/proc/net/wireless` にあります。
接続設定は `/config/wpa_supplicant.conf` ですが、この file は機微情報なので直接表示
しません。

`Connect Wi-Fi` は、A30 の stock `wpa_supplicant` init が `/config/wpa_supplicant.conf`
を読む制約に合わせ、成功時だけこの file を更新します。plumOS 側では
`/mnt/SDCARD/plumos/backups/network/` に事前 backup を作り、
`/mnt/SDCARD/plumos/config/network/wpa_supplicant.conf` に成功後の copy を保持します。
接続試行は `/tmp` の一時 file から `wpa_supplicant` を起動し、DHCP で IP を取得し、
default gateway があれば ping を 1 回実行します。IP 取得に失敗した場合は以前の
`/config/wpa_supplicant.conf` へ戻し、既存設定での再起動を試みます。

接続フローの順序は次の通りです。

- SSID 検索
- パスワード入力
- `wpa_supplicant` 起動
- DHCP で IP address 取得
- default gateway が存在すれば ping 1 回
- 接続完了画面に IP address を表示

## keymap/input

plumOS controller prototype は `gpio-keys-polled` の `/dev/input/event*` を直接読める
ことを確認しています。

`plumos-input-compare` では、stock `keymon` と stock `MainUI` が動作中でも
`/dev/input/event3` を非排他で直接 open/poll できることを確認しました。初期方針は
stock `keymon` を残しつつ、frontend は plumOS 側の input mapping を優先します。

## write-enabled UI の条件

A30 設定を書き換える UI は、以下を満たしてから実装します。

- 読み取り元と書き込み先を分離して説明できる
- 変更前 backup を必ず作る
- atomic write と `sync` を行う
- 変更後の runtime 反映を確認できる
- 失敗時に rollback できる
- SSH 作業中に recovery できる

この条件を満たした plumOS-owned な限定キーだけを write-enabled にします。
未検証 backend へ直接反映する処理は、対応関係を確認してから追加します。
