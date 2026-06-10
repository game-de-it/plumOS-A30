# plumOS 設定ファイル配置方針

plumOS に関する永続ファイルは `/mnt/SDCARD/plumos/` 配下に置きます。stockOS の
`/config` や stock frontend の設定ファイルは plumOS の通常設定として書き換えません。

## 基本方針

- 設定は巨大な `system.json` 1本に集約しない。
- UI、system、network、performance などの責務ごとに分ける。
- 設定ファイルは人が読める JSON にする。
- 保存は tmp file、`fsync`、atomic rename、`sync` を使う。
- 互換性のため、各ファイルに `version` を置く。
- 機微情報は通常の設定ファイルや git 管理対象に入れない。
- stockOS-owned な `/config/system.json` は参照・書き込み対象にしない。

## 現在の配置

- `/mnt/SDCARD/plumos/config/frontend/settings.json`
  - UI Mode、Graphic Theme、TOP/ROM list 表示、sort、scan、boot resume など frontend の挙動。
- `/mnt/SDCARD/plumos/config/frontend/menus.json`
  - START menu など、frontend menu 定義。
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - system 定義、ROM directory alias、launch profile、artwork lookup。
- `/mnt/SDCARD/plumos/config/system/settings.json`
  - plumOS-owned の本体設定。Volume、Brightness、Lumination、Display Color、Time Settings、Language。

## 今後の候補

- `/mnt/SDCARD/plumos/config/network/settings.json`
  - Wi-Fi enable 方針、network recovery 方針など。SSID/PSK は別管理にする。
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - 既存の system/ROM 別 launch profile、CPU policy/frequency/core override。
  - Performance Settings はこの既存機能へ接続済み。
- `/mnt/SDCARD/plumos/config/performance/profiles.json`
  - 将来、global preset や名前付き performance profile が必要になった場合に追加する。
- `/mnt/SDCARD/plumos/config/input/mapping.json`
  - plumOS input mapping、hotkey、物理ボタン連動。

## System Settings

System Settings は `/mnt/SDCARD/plumos/config/system/settings.json` を読み書きします。
stockOS の `/config/system.json` とは切り離します。

現在のキー:

- `volume`: `0..20`
- `brightness`: `1..20`
- `lumination`: `0..10`
- `contrast`: `0..20`
- `hue`: `0..20`
- `saturation`: `0..20`
- `language`: `en.lang`, `ja.lang` など
- `timezone`: POSIX TZ 文字列。既定値は `JST-9`

2026-06-10 時点では、`Language` 以外の値は保存直後と FE 起動時に A30
runtime へ反映します。`volume` は ALSA `Soft Volume Master`、`brightness` は
`/sys/devices/virtual/disp/disp/attr/lcdbl`、`lumination` / `contrast` / `hue` /
`saturation` は `/sys/devices/virtual/disp/disp/attr/enhance` を使います。
Brightness は `1..20` を保存し、RAW は
`2,3,4,5,6,7,8,9,10,26,43,59,75,92,108,125,141,157,174,190` へ割り当てます。
`timezone` は plumOS config を原本とし、保存時・FE 起動時・MainUI wrapper 起動時に
`TZ` 環境と runtime `/etc/TZ` へ反映します。stockOS の `/config/system.json` には
書き込みません。手動時刻設定は選択中の timezone のローカル時刻として入力し、
UTC epoch に変換して OS 時刻へ適用します。

## Performance Settings

Performance Settings は新しい cpufreq 仕様を先に作るのではなく、既存の
`plumos-text-ui core system|rom ... --cpu --freq --cores` と
`/mnt/SDCARD/plumos/state/frontend/core-overrides.json` に接続します。
`systems.json` の `default_cpu_policy` / `default_cpu_freq_khz` を既定値とし、
system override、ROM override の順で上書きします。plumOS default は全systemで
`648MHz` / `2 cores` に揃えます。

2026-06-09 時点では START `Performance Settings` から system を選び、
`CPU freq` と `CPU Cores` を左右で変更できます。`CPU freq` の候補は
`648/816/1200/1344 MHz` の固定値だけで、予測しづらい `keep` はユーザー向けから
削除します。保存は controller UI が
`plumos-text-ui core system <system> --cpu ... --freq ... --cores ...` を呼ぶことで
`core-overrides.json` へ入ります。`Reset to Default` は `--clear-cpu` を呼び、
`systems.json` の `648MHz` / `2 cores` default へ戻します。
