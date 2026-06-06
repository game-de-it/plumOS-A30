# A30 input policy

この文書は、stock `keymon` と plumOS の直接 input event 読み取りを比較した初期結果です。

結論として、現段階では stock `keymon` をすぐ停止せずに残します。ただし、
`/dev/input/event3` の直接読み取りは非排他で成立するため、plumOS frontend は直接
event を読む実装で進められます。

## 診断 binary

`plumos-input-compare` は `plumos-runtime-probe` package に含まれます。

build:

```sh
./scripts/docker-build.sh runtime-probe
```

deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

run:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

実機ボタン mapping を取る場合は、長めに実行して A30 本体のボタンを押します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 10000'
```

複数の input device を同時に確認したい場合は `--all-events` を使います。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --all-events --timeout-ms 30000'
```

## 2026-06-06 実機結果

```text
plumOS input compare
direct path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
input devices=4 selected=/dev/input/event3
device path=/dev/input/event0 name=axp22-supplyer handlers=kbd ddrfreq_dsm event0  gpio=no
device path=/dev/input/event1 name=headset handlers=kbd event1  gpio=no
device path=/dev/input/event2 name=sunxi-ths handlers=event2  gpio=no
device path=/dev/input/event3 name=gpio-keys-polled handlers=kbd ddrfreq_dsm event3  gpio=yes
process keymon_running=yes mainui_running=yes refs=7
process_ref pid=557 comm=keymon fd=5 target=/dev/input/event3
process_ref pid=557 comm=keymon fd=7 target=/dev/input/event2
process_ref pid=557 comm=keymon fd=8 target=/dev/input/event1
process_ref pid=557 comm=keymon fd=10 target=/dev/input/event0
process_ref pid=1230 comm=MainUI fd=4 target=/dev/input/event0
process_ref pid=1230 comm=MainUI fd=5 target=/dev/input/event3
process_ref pid=1230 comm=MainUI fd=6 target=/dev/input/event1
comparison keymon_holds_selected=yes mainui_holds_selected=yes direct_open=yes
decision=keep_keymon_for_now; direct_input_is_viable_nonexclusive
```

観測:

- `gpio-keys-polled` は `/dev/input/event3`
- stock `keymon` は `event0`, `event1`, `event2`, `event3` を開いている
- stock `MainUI` も `event0`, `event1`, `event3` を開いている
- その状態でも plumOS は `/dev/input/event3` を直接 open/poll できる
- 今回の 100ms 実行では物理ボタンを押していないため `events=0`
- `pid` は観測時の値であり、起動ごとに変わる

## 物理ボタン mapping

2026-06-06 に stock `keymon` と stock `MainUI` を動かしたまま
`plumos-input-compare --all-events` で確認しました。すべて
`/dev/input/event3` の `gpio-keys-polled` から観測されています。

| 物理ボタン | code | key name | probe action | frontend action |
| --- | ---: | --- | --- | --- |
| 上 | 103 | `KEY_UP` | `up` | cursor up |
| 下 | 108 | `KEY_DOWN` | `down` | cursor down |
| 左 | 105 | `KEY_LEFT` | `left` | cursor left/back |
| 右 | 106 | `KEY_RIGHT` | `right` | cursor right/open |
| A | 57 | `KEY_SPACE` | `a` | A/open |
| B | 29 | `KEY_LEFTCTRL` | `b` | B/back |
| X | 42 | `KEY_LEFTSHIFT` | `x` | reserved |
| Y | 56 | `KEY_LEFTALT` | `y` | reserved |
| L | 15 | `KEY_TAB` | `l` | reserved |
| R | 14 | `KEY_BACKSPACE` | `r` | reserved |
| L2 | 18 | `KEY_E` | `l2` | reserved |
| R2 | 20 | `KEY_T` | `r2` | reserved |
| 音量 - | 114 | `KEY_VOLUMEDOWN` | `volume_down` | reserved |
| 音量 + | 115 | `KEY_VOLUMEUP` | `volume_up` | reserved |
| Function | 1 | `KEY_ESC` | `function` | safe menu candidate |
| START | 28 | `KEY_ENTER` | `start` | START menu |
| SELECT | 97 | `KEY_RIGHTCTRL` | `select` | core menu |
| 左スティック軸 | - | - | - | kernel input には未露出 |
| 左スティック押し込み | - | - | - | 未確定 |

注意:

- START menu は物理 START (`KEY_ENTER`) で開きます。
- Function (`KEY_ESC`) は START の代替としては扱わず、safe shutdown/resume menu の
  第一候補として扱います。
- X/Y/L/R/L2/R2/音量ボタンは probe では識別しますが、現時点の controller UI では
  通常操作に割り当てません。
- 電源ボタンは stock 側または kernel 側の sleep/shutdown 介入を避けるため未確定です。
  plumOS の自動再開機能は電源ボタンに依存せず、Function button 代替を優先して設計します。

## 左アナログスティック

2026-06-06 に追加確認しました。結論として、左スティックは通常の
`/dev/input/event*` には analog axis として出ていません。

確認内容:

- `plumos-input-compare --all-events` に `EV_ABS` 表示を追加した
- `/proc/bus/input/devices` では stick 用の `ABS_X`/`ABS_Y` device は見つからない
- 左スティック操作中も `EV_ABS` は観測できなかった
- stock `MainUI.stock` は `/dev/mem` を mmap しており、binary 文字列に
  `JoypadCalibration`, `JoypadTest`, `ADC INFO`, `/config/joypad.config` がある
- stock `keymon` にも `/dev/mem`, `ABS_X`, `ABS_Y`, `BTN_THUMBL` などの文字列がある
- MainUI の Settings から calibration を実行すると `/config/joypad.config` が更新された

`/config/joypad.config` の観測値:

```text
before:
x_min=15
x_max=239
y_min=18
y_max=233
x_zero=124
y_zero=133

after calibration:
x_min=14
x_max=235
y_min=18
y_max=232
x_zero=126
y_zero=130
```

現時点の推定:

- 左スティック軸は kernel input event ではなく、stock `MainUI`/`keymon` が `/dev/mem`
  経由で ADC 値を読む実装の可能性が高い
- calibration は raw ADC 値の min/max/center を `/config/joypad.config` に保存する
- plumOS では stock library を流用せず、後続で `/dev/mem` mapping または同等の ADC 読み取り
  方法を特定する必要がある
- 左スティック押し込みは今回まだ確定できていないため、別途短時間の button capture を行う

## 方針

- 初期 frontend では stock `keymon` を残す
- plumOS frontend の操作入力は `/dev/input/event3` の直接読み取りで実装する
- stock MainUI と共存している間は `EVIOCGRAB` のような排他取得は使わない
- 電源ボタン以外の button code/action mapping は実機で確定済み
- safe shutdown/resume menu は電源キーではなく Function button から開く案を第一候補にする
- plumOS frontend を常用起動に切り替える段階で、`keymon` を残すか停止するか再判断する

現時点の推奨は「`keymon` は残すが、plumOS frontend は直接 input event を読む」です。
