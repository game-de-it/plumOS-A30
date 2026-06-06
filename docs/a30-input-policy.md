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
| 左スティック押し込み | - | - | - | kernel input/serial frame では未観測 |

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
- spruceOS の A30 実装では、`joystickinput` が `/dev/ttyS2` から raw data を読み、
  `/config/joypad.config` を適用して `/dev/input/event4` に analog event、
  `/dev/input/event3` に keyboard/D-pad event を送る構成になっている
- stock A30 の初期 `/dev` には `/dev/ttyS2` node が存在しないが、`/proc/tty/drivers`
  では `ttyS` minor 0-4 が利用可能として表示される
- stock A30 実機では `/dev/ttyS0` から `ff b1 b2 b3 b4 fe` 形式の6バイト frame を
  9600/8N1 で観測できた

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

`plumos-serial-joy-probe --port /dev/ttyS0 --stats-only` の観測値:

```text
center, 1s:
frames=67
axisYL min=116 max=117 avg=116.03
axisXL min=103 max=107 avg=105.03
axisYR min=121 max=121 avg=121.00
axisXR min=124 max=124 avg=124.00

left stick moved, 12s:
frames=799
axisYL min=27 max=201 avg=113.53
axisXL min=45 max=157 avg=102.03
axisYR min=14 max=245 avg=121.45
axisXR min=15 max=232 avg=121.65
```

左スティック押し込みの確認:

```text
plumos-input-compare --all-events, 8s/12s while pressing:
events=0 key_events=0 abs_events=0

plumos-serial-joy-probe --timeout-ms 5000 --frames-only while pressing:
frames=333
axisYL min=116 max=119 avg=117.25
axisXL min=102 max=107 avg=104.82
axisYR min=124 max=124 avg=124.00
axisXR min=127 max=127 avg=127.00
```

押し込み設定保存の確認:

- stockOS の `/config/joypad.config` は `x_min`, `x_max`, `y_min`, `y_max`,
  `x_zero`, `y_zero` の6項目のみ
- stockOS の `/config/system.json` は通常ボタン向けの `keymap` を持つが、
  左スティック押し込み用の項目は見当たらない
- stock `MainUI` の文字列には `JoypadCalibration`, `JoypadTest`,
  `/config/joypad.config`, `x_min`/`x_max`/`y_min`/`y_max`/`x_zero`/`y_zero`
  があるが、押し込みを保存する項目は見当たらない
- stock `keymon` の文字列には `BTN_THUMBL`/`BTN_THUMBR` があるが、Linux input の
  汎用 key name table 由来の可能性が高く、実機 event や保存項目の証拠としては弱い
- spruceOS の `run_analog_stick_calibration` も X/Y 軸の min/max/center だけを
  `joypad.config` へ書き、押し込みは sample/save していない

現時点の推定:

- 左スティック軸は kernel input event ではなく、serial raw data を userland daemon が
  virtual input に変換する構成の可能性が高い
- A30 実機では spruceOS の `/dev/ttyS2` ではなく `/dev/ttyS0` が raw data 経路の
  本命。`axisYR`/`axisXR` が `/config/joypad.config` の min/max に近く、実際の
  左スティック X/Y に対応している可能性が高い
- `/dev/mem` は stock `MainUI` の calibration/test 画面または別ハード制御で使われている
  可能性があるため、stick の本命経路としては一段下げる
- calibration は raw 値の min/max/center を `/config/joypad.config` に保存する
- plumOS では stock/spruce の binary を流用せず、`/dev/ttyS0` の raw data を読む
  `plumos-joystickd` を後続で設計する
- 左スティック押し込みは通常の kernel input event と `/dev/ttyS0` の6バイト serial frame
  には出ていない。stockOS/spruceOS の設定保存にも押し込み項目が見当たらないため、
  初期 plumOS では未接続/未対応として扱う

## PPSSPP の analog input 経路

stock PPSSPP を MainUI から起動し、左スティックが PPSSPP 内で動作している状態で確認しました。

再現用 script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-input.sh
```

確認結果:

- PPSSPP の `launch.sh` は `./miyoo282_xpad_inputd&` を起動してから
  `./PPSSPPSDL "$*"` を実行する
- `miyoo282_xpad_inputd` は `/dev/uinput` と `/dev/ttyS0` を開く
- `miyoo282_xpad_inputd` の binary 文字列には `/config/joypad.config`,
  `/dev/ttyS0`, `MIYOO Pad1`, `/dev/uinput` がある
- PPSSPP 起動中は `MIYOO Pad1` という virtual input device が追加される
- `MIYOO Pad1` は `045e:028e`, `js0`/`event4`, 8 axes / 11 buttons の
  Xbox 360 互換風 composite device として見える
- `PPSSPPSDL` は `/dev/input/event4` を開き、`libSDL2-2.0.so.0` と
  `SDL_GameController*` / `SDL_Joystick*` API を使う
- 左スティック押し込みは PPSSPP の controller 設定でも反応しなかった

判断:

- standalone emulator 向けの analog 経路は axes-only よりも buttons+axes の
  composite virtual pad が有利
- stock `miyoo282_xpad_inputd` は流用せず、plumOS では同じ原理を
  `plumos-joystickd` の mode として実装する
- RetroArch も stock SDL1 経路に合わせるより、plumOS build の SDL2/evdev と
  composite virtual pad で検証する
- 左スティック押し込みは、PPSSPP でも反応しないため引き続き未対応扱いにする

## 方針

- 初期 frontend では stock `keymon` を残す
- plumOS frontend の操作入力は `/dev/input/event3` の直接読み取りで実装する
- stock MainUI と共存している間は `EVIOCGRAB` のような排他取得は使わない
- 電源ボタン以外の button code/action mapping は実機で確定済み
- safe shutdown/resume menu は電源キーではなく Function button から開く案を第一候補にする
- plumOS frontend を常用起動に切り替える段階で、`keymon` を残すか停止するか再判断する
- 左スティック押し込みは初期 mapping に含めない。新しい証拠が出た場合だけ再調査する
- emulator 向け analog stick は `plumos-joystickd` の composite virtual pad mode を
  優先して検証する
- stock PPSSPP を `miyoo282_xpad_inputd` なしで直接起動し、`plumOS A30 Gamepad` が
  SDL2 GameController mapping 成功で pad 1 に割り当てられることを確認済み
- stock RetroArch/SDL1 では axes-only と composite gamepad のどちらも Linux joystick
  API からは見えるが、RetroArch log では autoconfig/接続を確認できない。RetroArch は
  plumOS build の SDL2/evdev + composite virtual pad を優先する
- `plumos-joystickd --device-mode xbox` の button forwarding では Function も
  `BTN_MODE` として転送できるため、emulator 実行中の safe menu 入力候補にできる
- plumOS 同梱 upstream SDL3 3.4.10 + sdl2-compat 2.32.68 の probe でも、
  `plumos-joystickd --device-mode xbox` の composite virtual pad が SDL2
  GameController として自動認識されることを確認済み
- `scripts/probe-a30-joystickd-residency.sh` で、stockless plumOS 状態のまま
  `plumos-joystickd --device-mode xbox` を常駐させ、FE は `/dev/input/event3` を
  非排他で直接読み、SDL2 probe と PPSSPP direct launch は `plumOS A30 Gamepad`
  (`event4`) を GameController として認識することを確認済み
- 上記常駐 probe の終了後、`plumos-joystickd` process、`plumOS A30 Gamepad` device、
  `/dev/uinput`/`event4`/`ttyS0` fd は残らなかった
- PPSSPP direct launch は plumOS gamepad の `event4` に加え、`/dev/input/event3` と
  `/dev/ttyS0` も開く。物理操作時の二重入力や serial read 競合がないかは追加確認する

現時点の推奨は「plumOS 常用時は stock `keymon` を止め、frontend は
`/dev/input/event3` を直接読み、emulator 向けには `plumos-joystickd --device-mode xbox`
常駐を第一候補にする」です。PPSSPP は `event3`/`ttyS0` の追加 open を確認してから
default launch profile 化します。
