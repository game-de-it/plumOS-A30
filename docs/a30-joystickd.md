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
- 左スティック押し込みの event source

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
S: Sysfs=/devices/virtual/input/input4
H: Handlers=js0 event4
B: EV=b
B: KEY=20000000 0 0 0 0 0 0 0 0 0
B: ABS=3
```

`plumos-joystickd` は A30 実機で `/dev/uinput` から virtual input device を作成できました。
次は RetroArch/SDL からこの `js0`/`event4` が見えるかを検証します。

## options

- `--serial PATH`: serial raw stick path。既定値は `/dev/ttyS0`
- `--calibration PATH`: calibration file。既定値は `/config/joypad.config`
- `--uinput PATH`: uinput path。既定値は `/dev/uinput`
- `--timeout-ms MS`: 指定時間後に終了。既定値 `0` は signal まで常駐
- `--no-uinput`: virtual input を作らず、読み取り/正規化だけ確認
- `--x-source NAME`: `axisYL`, `axisXL`, `axisYR`, `axisXR`
- `--y-source NAME`: `axisYL`, `axisXL`, `axisYR`, `axisXR`
- `--invert-x`, `--invert-y`: 正規化後の符号を反転
- `--deadzone-raw N`: raw center 周辺の deadzone。既定値は `8`
- `--print-every N`: N frame ごとに raw/normalized 値を表示

## 実装方針

- frontend は当面 `/dev/input/event3` の button event を直接読む
- `plumos-joystickd` は RetroArch/standalone emulator 用の analog stick 経路として扱う
- emulator 起動中だけ daemon を有効化する運用も検討する
- 最終的には plumOS の launch profile から「analog daemon が必要か」を選べるようにする
