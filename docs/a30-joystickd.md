# A30 joystickd

`plumos-joystickd` は、Miyoo A30 の serial raw joystick frame を Linux の
virtual input device に変換する実験用 daemon です。

目的:

- `/dev/ttyS0` から 9600/8N1 の `ff b1 b2 b3 b4 fe` frame を読む
- `/config/joypad.config` の calibration 値を適用する
- `/dev/uinput` で `ABS_X`/`ABS_Y` を持つ virtual analog stick を作る
- RetroArch や standalone emulator から通常の Linux input device として見せる

stock/spruce の binary は流用しません。

## build

```sh
./scripts/docker-build.sh joystickd
```

生成物:

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

## deploy/run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
```

読み取りと正規化だけを確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

短時間だけ virtual stick を作る:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --timeout-ms 2000 --print-every 50'
```

PPSSPP 型の composite gamepad を短時間だけ作る:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --device-mode xbox --timeout-ms 5000'
```

再現用 script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-joystickd-xbox.sh --deploy
```

この mode は `/dev/ttyS0` の左スティック軸と `/dev/input/event3` の物理ボタンを
1つの virtual gamepad へ合成します。常駐候補ですが、FE/`keymon`/emulator での
二重入力がないことを確認してから default service 化します。

daemon が作った `js0`/`event4` を Linux joystick API と evdev で読む:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --timeout-ms 3000 >/tmp/plumos-joystickd.log 2>&1 & sleep 0.5; /mnt/SDCARD/plumos/bin/plumos-joystick-reader --timeout-ms 1000; wait; cat /tmp/plumos-joystickd.log; rm -f /tmp/plumos-joystickd.log'
```

## default mapping

現在の仮設定:

| virtual input | raw source | calibration |
| --- | --- | --- |
| `ABS_X` | `axisXR` | `x_min/x_max/x_zero` |
| `ABS_Y` | `axisYR` | `y_min/y_max/y_zero` |

根拠:

- `/dev/ttyS0` の frame は `ff axisYL axisXL axisYR axisXR fe` として解釈している
- 実機で左スティックを回したとき、`axisYR`/`axisXR` の min/max が
  `/config/joypad.config` の値に近かった

未確定:

- `axisXR` が物理 X で正しいか
- `axisYR` が物理 Y で正しいか
- 上下左右の符号

左スティック押し込みは、stockOS/spruceOS の設定保存内容と実機 event 結果から
初期 plumOS では未接続/未対応として扱います。

これらは個別 capture で確定します。それまでは `--x-source`, `--y-source`,
`--invert-x`, `--invert-y` で調整可能にします。

## 2026-06-06 実機結果

読み取りと正規化:

```text
plumOS joystickd
serial=/dev/ttyS0 baud=9600 calibration=/config/joypad.config uinput=/dev/uinput no_uinput=yes x_source=axisXR y_source=axisYR invert_x=no invert_y=no deadzone_raw=8 timeout_ms=1000
calibration path=/config/joypad.config open=yes fields=6 x_min=14 x_max=235 x_zero=126 y_min=18 y_max=232 y_zero=130
serial path=/dev/ttyS0 tcsetattr=yes raw_8n1=yes
frame=20 raw axisYL=118 axisXL=106 axisYR=127 axisXR=127 normalized x=0 y=0 emitted=0
summary frames=67 emitted=0 last_x=0 last_y=0 duration_ms=1013
```

uinput device:

```text
I: Bus=0003 Vendor=706c Product=4130 Version=0001
N: Name="plumOS A30 Analog Stick"
S: Sysfs=/devices/virtual/input/input9
H: Handlers=js0 event4
B: PROP=0
B: EV=9
B: ABS=3
```

`plumos-joystickd` は A30 実機で `/dev/uinput` から axes-only の virtual input device を
作成できました。押し込み未対応方針に合わせ、仮の `BTN_THUMBL` は出していません。

## Linux joystick API / evdev 確認

`plumos-joystick-reader` で、daemon が作った `js0`/`event4` を読み取れることを
A30 実機で確認しました。

stick を動かさずに open/init だけ確認した結果:

```text
plumOS joystick reader
js=/dev/input/js0 event=/dev/input/event4 name="plumOS A30 Analog Stick" timeout_ms=1000 no_js=no no_event=no
js path=/dev/input/js0 open=yes
evdev path=/dev/input/event4 open=yes
js event index=1 time=328840 type=0x82 type_name=JS_AXIS_INIT number=0 value=0
js event index=2 time=328840 type=0x82 type_name=JS_AXIS_INIT number=1 value=-669
summary js_events=2 evdev_events=0 timeout_ms=1000
```

実際に左スティックを動かした 10 秒確認では、Linux joystick API と evdev の両方で
event を確認しました。

```text
summary js_events=370 evdev_events=759 timeout_ms=10000
```

このため、`plumos-joystickd` が作る device は Linux joystick API/evdev から見えると
判断します。次は RetroArch/SDL からの認識確認へ進みます。

左スティック押し込みは `plumos-input-compare --all-events` と `/dev/ttyS0` serial frame の
確認では観測できておらず、stockOS/spruceOS の保存項目にも見当たりません。このため、
初期 plumOS の analog stick daemon は X/Y 軸だけを提供します。

## RetroArch / SDL 確認

stock RetroArch 1.16.0 の build feature は以下でした。

```text
SDL             - SDL input/audio/video drivers: yes
SDL2            - SDL2 input/audio/video drivers: no
UDEV            - UDEV/EVDEV input driver: no
```

stock `retroarch.cfg` は `input_driver = "sdl"`、
`input_joypad_driver = "sdl"` です。そのため stock RetroArch では SDL1 joystick 経路が
確認対象になります。

再現用 script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --deploy
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --device-mode analog
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --device-mode xbox
```

2026-06-06 の確認結果:

```text
js info path=/dev/input/js0 name="plumOS A30 Analog Stick" axes=2 buttons=0
evdev info path=/dev/input/event4 name="plumOS A30 Analog Stick"
[INFO] [Input]: Found input driver: "sdl".
[INFO] [Joypad]: Found joypad driver: "sdl".
result=retroarch_sdl_driver_only
```

`sdl` と `sdl_dingux` の一時 autoconfig を試しましたが、RetroArch log には
`plumOS A30 Analog Stick` の configured/connection log は出ませんでした。

同じ script を `--device-mode xbox` に拡張し、`plumOS A30 Gamepad` でも確認しました。

```text
detected mode=xbox name="plumOS A30 Gamepad" js=/dev/input/js0 event=/dev/input/event4
js info path=/dev/input/js0 name="plumOS A30 Gamepad" axes=8 buttons=11
evdev info path=/dev/input/event4 name="plumOS A30 Gamepad"
[INFO] [Input]: Found input driver: "sdl".
[INFO] [Joypad]: Found joypad driver: "sdl".
result=retroarch_sdl_driver_only
```

stock RetroArch の stdout には `joystick read cal` と `thread serial joystick` が出ますが、
RetroArch log には axes-only/composite のどちらでも `plumOS A30 ...` device の
configured/connection log は出ませんでした。

MainUI から stock RetroArch を起動した手動確認では、Port1 Controls の bind 待ち受けで
左スティックが `Axis -2`/`±2` として検出される一方、実際に左スティックを動かしても
メニューカーソルは移動しませんでした。SSH 側で確認した stock RetroArch process は
`libSDL-1.2.so.0` を map していましたが、開いていた input fd は `/dev/input/event0`
のみで、`/dev/input/js*` や `event4` は開いていませんでした。

現時点の判断:

- `plumos-joystickd` の virtual device は Linux joystick API/evdev では正常に見える
- stock RetroArch の SDL1 joypad driver は有効だが、log 上は axes-only `js0` と
  composite `plumOS A30 Gamepad` のどちらも autoconfig/接続を確認できていない
- stock RetroArch は bind 待ち受けで軸らしき入力を拾うが、通常操作には使えていない
- RetroArch 向け analog strategy は stock SDL1 挙動を前提にせず、plumOS RetroArch
  build の SDL2/evdev 対応と buttons+axes の composite virtual pad を優先する

## PPSSPP / SDL2 GameController 確認

stock PPSSPP を MainUI から起動し、左スティックが動作している状態で確認しました。

再現用 script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-input.sh
```

`/mnt/SDCARD/Emu/PPSSPP/launch.sh` は PPSSPP 起動前に専用 daemon を起動しています。

```sh
./miyoo282_xpad_inputd&
./PPSSPPSDL "$*"
killall miyoo282_xpad_inputd
```

PPSSPP 起動中の process:

```text
/mnt/SDCARD/Emu/PPSSPP/launch.sh
./miyoo282_xpad_inputd
./PPSSPPSDL /mnt/SDCARD/Emu/PPSSPP/../../Roms/PSP/Puzzle_Bobble.cso
```

`/proc/bus/input/devices` には次の virtual pad が追加されます。

```text
I: Bus=0003 Vendor=045e Product=028e Version=045e
N: Name="MIYOO Pad1"
S: Sysfs=/devices/virtual/input/input18
H: Handlers=js0 event4
B: EV=2b
B: KEY=7cdb0000 0 0 0 0 0 0 0 0 0
B: ABS=3003f
```

`plumos-joystick-reader` で読むと、`MIYOO Pad1` は 8 axes / 11 buttons の
composite device として見えました。

```text
js info path=/dev/input/js0 name="MIYOO Pad1" axes=8 buttons=11
evdev info path=/dev/input/event4 name="MIYOO Pad1"
```

process fd の観測:

```text
miyoo282_xpad_inputd -> /dev/uinput
miyoo282_xpad_inputd -> /dev/ttyS0
PPSSPPSDL -> /dev/input/event4
PPSSPPSDL -> /dev/ttyS0
```

`miyoo282_xpad_inputd` の文字列には以下がありました。

```text
/config/joypad.config
/dev/ttyS0
MIYOO Pad1
/dev/uinput
```

`PPSSPPSDL` は `libSDL2-2.0.so.0` と `SDL_GameController*` /
`SDL_Joystick*` API を使っており、binary 内の path から PPSSPP 1.16.6 系と
判断できます。`assets/gamecontrollerdb.txt` には Xbox 360 controller mapping が
含まれており、`MIYOO Pad1` の `045e:028e` はこの系統の mapping に乗せている
可能性が高いです。

現時点の判断:

- PPSSPP で analog stick が動く理由は、PPSSPP 本体が raw serial を直接処理している
  からではなく、起動 script が `miyoo282_xpad_inputd` を起動して composite virtual
  pad を作っているため
- `miyoo282_xpad_inputd` は `/dev/ttyS0` と `/config/joypad.config` を読み、
  `/dev/uinput` で Xbox 360 互換風の `MIYOO Pad1` を作る
- PPSSPP は SDL2 GameController/Joystick 経路で `/dev/input/event4` の
  `MIYOO Pad1` を読む
- 左スティック押し込みは PPSSPP の controller 設定でも反応しなかったため、引き続き
  未接続/未対応として扱う
- plumOS では stock binary を流用せず、`plumos-joystickd` を axes-only から
  buttons+axes の composite virtual pad mode へ拡張する案を優先する
- plumOS RetroArch は stock SDL1 経路に依存せず、SDL2/evdev + composite virtual pad
  を優先して検証する

## composite gamepad mode

`plumos-joystickd` には 2026-06-06 時点で `--device-mode xbox` を追加しました。
この mode は stock `miyoo282_xpad_inputd` を流用せず、PPSSPP の観測結果から
plumOS 側で同じ原理を実装するものです。

作る device:

```text
name="plumOS A30 Gamepad"
id=045e:028e
axes=ABS_X,ABS_Y,ABS_Z,ABS_RX,ABS_RY,ABS_RZ,ABS_HAT0X,ABS_HAT0Y
buttons=BTN_A,BTN_B,BTN_X,BTN_Y,BTN_TL,BTN_TR,BTN_SELECT,BTN_START,BTN_MODE,BTN_THUMBL,BTN_THUMBR
```

mapping:

| source | virtual output |
| --- | --- |
| `/dev/ttyS0` X/Y | `ABS_X` / `ABS_Y` |
| D-pad | `ABS_HAT0X` / `ABS_HAT0Y` |
| A/B/X/Y | `BTN_A` / `BTN_B` / `BTN_X` / `BTN_Y` |
| L/R | `BTN_TL` / `BTN_TR` |
| L2/R2 | `ABS_Z` / `ABS_RZ` digital trigger value |
| START/SELECT | `BTN_START` / `BTN_SELECT` |
| Function | `BTN_MODE` |
| left-stick click | 未対応。button capability はあるが event は出さない |

`--button-event PATH` で物理ボタンの input event を指定できます。既定値は
`/dev/input/event3` です。`--no-buttons` を付けると、左スティック軸だけを
composite gamepad に流します。

常駐方針:

- plumOS 常用時は `--device-mode xbox` の常駐を第一候補にする
- stock MainUI/PPSSPP と同時に動かすと input device が複数増えるため、調査時は
  `--timeout-ms` 付きで短時間確認する
- plumOS frontend は自分の操作用には引き続き `/dev/input/event3` を直接読み、
  emulator には composite gamepad を渡す構成を優先する
- 常駐を default にする前に、FE/`keymon`/RetroArch/standalone emulator で二重入力や
  stale fd が残らないか確認する

2026-06-06 実機結果:

stock PPSSPP が起動しており、stock 側の `MIYOO Pad1` が `js0`/`event4` にいる状態で
`plumos-joystickd --device-mode xbox --timeout-ms 5000` を実行しました。

```text
detected js=/dev/input/js1 event=/dev/input/event5
N: Name="plumOS A30 Gamepad"
H: Handlers=js1 event5
B: EV=b
B: KEY=7cdb0000 0 0 0 0 0 0 0 0 0
B: ABS=3003f
js info path=/dev/input/js1 name="plumOS A30 Gamepad" axes=8 buttons=11
evdev info path=/dev/input/event5 name="plumOS A30 Gamepad"
summary frames=196 emitted=1 button_events=0 last_x=0 last_y=0 duration_ms=5133
```

これにより、`xbox` mode の virtual device capability は PPSSPP で観測した
`MIYOO Pad1` と同系統の 8 axes / 11 buttons として A30 実機に作成できることを
確認しました。次は SDL2 GameController と RetroArch/standalone emulator からの
認識確認を行います。

### PPSSPP direct launch 確認

PPSSPP を停止した状態では、`plumOS A30 Gamepad` は `js0`/`event4` に作成されます。
その状態で stock `miyoo282_xpad_inputd` を起動せず、`PPSSPPSDL` だけを直接起動して
確認しました。

再現用 script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-plumos-gamepad.sh
```

確認結果:

```text
N: Name="plumOS A30 Gamepad"
H: Handlers=js0 event4
B: EV=b
B: KEY=7cdb0000 0 0 0 0 0 0 0 0 0
B: ABS=3003f
PPSSPPSDL -> /dev/input/event4
```

PPSSPP log:

```text
loading control pad mappings from gamecontrollerdb.txt: SUCCESS!
found control pad: Atari Xbox 360 Game Controller, loading mapping: SUCCESS
pad 1 has been assigned to control pad: Atari Xbox 360 Game Controller
```

この結果から、`plumos-joystickd --device-mode xbox` は stock PPSSPP の SDL2
GameController 経路から認識され、GameController mapping も成功すると判断します。
PPSSPP は process fd と log の両方で plumOS gamepad を使っていることを確認済みです。
PPSSPP direct launch と stock RetroArch probe の終了後、`plumos-joystickd` process、
`plumOS A30 Gamepad` device、`/dev/uinput`/`event4`/`ttyS0` fd が残らないことも
確認しました。

## options

- `--serial PATH`: serial raw stick path。既定値は `/dev/ttyS0`
- `--calibration PATH`: calibration file。既定値は `/config/joypad.config`
- `--uinput PATH`: uinput path。既定値は `/dev/uinput`
- `--device-mode MODE`: `analog` または `xbox`。既定値は `analog`
- `--button-event PATH`: `xbox` mode で合成する物理ボタン event。既定値は
  `/dev/input/event3`
- `--no-buttons`: `xbox` mode で物理ボタンを合成しない
- `--timeout-ms MS`: 指定時間後に終了。既定値 `0` は signal まで常駐
- `--no-uinput`: virtual input を作らず、読み取り/正規化だけ確認
- `--x-source NAME`: `axisYL`, `axisXL`, `axisYR`, `axisXR`
- `--y-source NAME`: `axisYL`, `axisXL`, `axisYR`, `axisXR`
- `--invert-x`, `--invert-y`: 正規化後の符号を反転
- `--deadzone-raw N`: raw center 周辺の deadzone。既定値は `8`
- `--print-every N`: N frame ごとに raw/normalized 値を表示

## joystick-reader options

- `--js PATH`: joystick API path。既定値は `/dev/input/js0`
- `--event PATH`: evdev path。未指定時は device name から自動検出し、fallback は
  `/dev/input/event4`
- `--name NAME`: evdev 自動検出に使う device name。既定値は `plumOS A30 Analog Stick`
- `--timeout-ms MS`: 監視時間
- `--no-js`, `--no-event`: 片方だけ確認したい場合に使う

## 実装方針

- frontend は当面 `/dev/input/event3` の button event を直接読む
- `plumos-joystickd` は RetroArch/standalone emulator 用の analog stick 経路として扱う
- `analog` mode は `ABS_X`/`ABS_Y` の axes-only device、`xbox` mode は
  emulator 向けの buttons+axes composite virtual pad として扱う
- 左スティック押し込みは初期 plumOS では event を出さない
- emulator 起動中だけ daemon を有効化する運用も検討する
- stock RetroArch/SDL1 には寄せず、plumOS RetroArch build の SDL2/evdev と
  `xbox` mode composite pad を優先して検証する
- 最終的には plumOS の launch profile から「input daemon が必要か」と device mode を
  選べるようにする
