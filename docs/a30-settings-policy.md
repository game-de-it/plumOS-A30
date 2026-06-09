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
- Wi-Fi の SSID、PSK、`/config/wpa_supplicant.conf` の内容は git、log、UI に出さない。

## controller UI の現在値表示

`plumos-controller-ui` の START menu から System Settings を開くと、ユーザーが
調整対象として認識する次の項目を表示します。2026-06-09 時点では
`/mnt/SDCARD/plumos/config/system/settings.json` を読み書きし、stockOS の
`/config/system.json` には触れません。`Language` と `Theme` 以外は、plumOS 設定保存後に
A30 runtime backend へ即時反映します。

- `Volume`: `volume`。左右で `0..20` を変更し、ALSA `Soft Volume Master` へ反映する。将来は物理音量ボタンと連動する
- `Brightness`: `brightness`。左右で `1..10` を変更し、`/sys/devices/virtual/disp/disp/attr/lcdbl` へ反映する。`0` は画面消灯相当で操作不能になるため使わない。将来は START + 音量ボタンなどの hotkey と連動する
- `Lumination`: `lumination`。左右で `0..10` を変更し、display `enhance` へ反映する
- `Display Color`: A でサブ項目を開き、`Contrast`, `Hue`, `Saturation` をそれぞれ `0..20` で変更する
- `Language`: `language`。左右で `English`, `Japanese`, `Chinese`, `Traditional Chinese`, `Korean`, `Spanish`, `Portuguese` を選択する
- `Theme`: graphical mode 向けの theme 設定候補。候補名と path の扱いが固まるまで read-only
- `INFORMATION`: 現在値や backend/policy などの情報系サブ項目

`INFORMATION` サブ項目には以下の read-only 情報を置きます。

- `Device Model`: 固定値 `Miyoo A30`
- `Linux Kernel`: `/proc/sys/kernel/osrelease`
- `SD Card`: `statvfs()` で見た空き容量/総容量
- `plumOS System Config`: `/mnt/SDCARD/plumos/config/system/settings.json` の読み取り状態
- `Input Device`: `gpio-keys-polled` から見つけた `/dev/input/event*`
- `Theme Source`: plumOS theme id
- `Audio Backend`: 検出した mixer backend。A30 では `Soft Volume Master (amixer)`
- `Display Backend`: 検出した display backend。A30 では `disp attr lcdbl/enhance`
- `Write Policy`: stockOS から切り離し、plumOS 配下だけへ保存する方針

Network Settings では次の項目を扱います。

- `Wi-Fi`: plumOS runtime
- `Connection`: `/tmp/wpa_status.txt` の `wpa_state`
- `IP Address`: `/tmp/wpa_status.txt` の `ip_address`
- `Signal`: `/tmp/wpa_status.txt` の `RSSI`
- `Link Speed`: `/tmp/wpa_status.txt` の `LINKSPEED`
- `Frequency`: `/tmp/wpa_status.txt` の `FREQUENCY`
- `SSH`: plumOS の remote access path。現状は Dropbear port 2222
- `Status Source`: runtime status の読み取り元
- `Config Source`: plumOS network runtime
- `Credentials`: `hidden`
- `Run Network Recovery`: A ボタンで Wi-Fi、DHCP、SSH recovery を実行する
- `Write Policy`: safe Wi-Fi editor 実装まで read-only

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
`/sys/devices/virtual/disp/disp/attr/enhance` へ反映します。`brightness 1..10` は、
線形では高域が早く飽和するため `lcdbl 12,18,26,36,50,68,90,118,160,255` の
体感寄せ table にします。`lumination 0..10` は `enhance` の第2値 `0..50`、
`contrast` / `hue` / `saturation 0..20` は `enhance` の第3-5値 `0..100` に丸めます。

## volume

plumOS 側では `/mnt/SDCARD/plumos/config/system/settings.json` に `volume` を持ちます。
UI からは plumOS 側の `volume` を backup 付き atomic write で更新し、保存直後に
`amixer cset iface=MIXER,name='Soft Volume Master'` へ反映します。`volume 0..20` は
mixer 値 `0..255` に丸めます。物理音量ボタン連動は別タスクです。

## Wi-Fi

runtime 状態は `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, `/proc/net/wireless` にあります。
接続設定は `/config/wpa_supplicant.conf` ですが、この file は機微情報なので直接表示
しません。

write 対応を行う場合は、少なくとも次を満たしてから実装します。

- 既存の Wi-Fi power sequence を再現できること
- 設定 file を backup できること
- 一時 file へ書いてから atomic rename できること
- service restart 後に DHCP と status 更新を確認できること
- 失敗時に以前の設定へ戻せること

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
