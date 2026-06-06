# A30 設定 UI 方針

この文書は plumOS frontend で扱う A30 固有設定の初期方針です。

目的は、stock frontend の仕様を無条件に流用することではありません。まず A30 の
実機状態を安全に観測し、どの backend を使うべきか検証したうえで、plumOS 側の
設定 UI と service に接続します。

## 基本方針

- plumOS frontend 設定と A30 本体設定は分離する。
- plumOS frontend 設定は `/mnt/SDCARD/plumos/config/frontend/settings.json` に置く。
- A30 本体設定の観測元は当面 `/config/system.json` と runtime status file にする。
- 初期 UI は read-only inventory とし、A30 設定を書き換えない。
- stock 側の仕様を採用する場合は、事前に理由とリスクを確認する。
- Wi-Fi の SSID、PSK、`/config/wpa_supplicant.conf` の内容は git、log、UI に出さない。

## controller UI の現在値表示

`plumos-controller-ui` の START menu から Settings を開くと、plumOS frontend 設定と
theme 状態に加えて、以下の A30 情報を表示します。

- `A30 Policy`: read-only inventory
- `A30 Write Policy`: write 対応を backend 検証後に延期する方針
- `A30 Config`: `/config/system.json` の読み取り状態
- `A30 Volume`: `vol`, `mute`, `bgmvol`
- `A30 Volume Backend`: `amixer` の有無から見た候補
- `A30 Brightness`: `brightness`, `lumination`
- `A30 Display Color`: `contrast`, `hue`, `saturation`
- `A30 Brightness Backend`: backlight/lcd sysfs の候補
- `A30 Wi-Fi Config`: `/config/system.json` の `wifi`
- `A30 Wi-Fi Runtime`: `wpa_state`, IP, RSSI, link speed, frequency
- `A30 Keymap`: `/config/system.json` の `keymap`
- `A30 Input Event`: `gpio-keys-polled` から見つけた `/dev/input/event*`
- `A30 Language`: `/config/system.json` の `language`
- `A30 Stock Theme`: stock theme path
- `A30 CPU Mode`: `/config/system.json` の `cpufreq`

Wi-Fi runtime は `/tmp/wpa_status.txt` から必要な key だけを読みます。SSID や PSK は
読みません。

## brightness

観測した A30 では `/sys/class/backlight` に利用できる brightness file がありません。
一方で `/config/system.json` には `brightness`, `lumination`, `contrast`, `hue`,
`saturation` が存在します。

当面は UI に現在値を表示するだけにします。書き込み対応は、stock frontend がどの
kernel/sysfs/API に反映しているかを確認してから実装します。

## volume

`/config/system.json` には `vol`, `mute`, `bgmvol` があります。実機には `amixer` も
存在するため、ALSA mixer 経由で直接制御できる可能性があります。

ただし mixer control の対応関係は未検証です。write 対応前に、`amixer contents` と
実際の音量変化を見て、設定値と mixer の対応を決めます。

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

stock frontend は `/config/system.json` の `keymap` と `keymon` を使っています。
plumOS controller prototype は `gpio-keys-polled` の `/dev/input/event*` を直接読める
ことを確認しています。

今後は stock `keymon` を残す場合と、SDL/input event を直接扱う場合を比較します。
直接入力が安定するなら、frontend は plumOS 側の input mapping を優先します。

## write-enabled UI の条件

A30 設定を書き換える UI は、以下を満たしてから実装します。

- 読み取り元と書き込み先を分離して説明できる
- 変更前 backup を必ず作る
- atomic write と `sync` を行う
- 変更後の runtime 反映を確認できる
- 失敗時に rollback できる
- SSH 作業中に recovery できる

この段階までは Settings 画面の A ボタンは edit preview のままにします。
