# A30 joystickd

`plumos-joystickd` is an experimental daemon that converts Miyoo A30 serial raw
joystick frames into a Linux virtual input device.

Goals:

- Read 9600/8N1 `ff b1 b2 b3 b4 fe` frames from `/dev/ttyS0`.
- Apply calibration from `/config/joypad.config`.
- Create a virtual analog stick with `ABS_X`/`ABS_Y` through `/dev/uinput`.
- Expose the stick to RetroArch and standalone emulators as a normal Linux
  input device.

It does not reuse stock/spruce binaries.

## Build

```sh
./scripts/docker-build.sh joystickd
```

Outputs:

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

## Deploy/Run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
```

Read and normalize only:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

Create a virtual stick briefly:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --timeout-ms 2000 --print-every 50'
```

Read the daemon-created `js0`/`event4` device through the Linux joystick API and
evdev:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --timeout-ms 3000 >/tmp/plumos-joystickd.log 2>&1 & sleep 0.5; /mnt/SDCARD/plumos/bin/plumos-joystick-reader --timeout-ms 1000; wait; cat /tmp/plumos-joystickd.log; rm -f /tmp/plumos-joystickd.log'
```

## Default Mapping

Current provisional mapping:

| virtual input | raw source | calibration |
| --- | --- | --- |
| `ABS_X` | `axisXR` | `x_min/x_max/x_zero` |
| `ABS_Y` | `axisYR` | `y_min/y_max/y_zero` |

Reasoning:

- `/dev/ttyS0` frames are interpreted as `ff axisYL axisXL axisYR axisXR fe`.
- When the stick was moved on hardware, `axisYR`/`axisXR` min/max values closely
  matched `/config/joypad.config`.

Unconfirmed:

- Whether `axisXR` is the physical X axis.
- Whether `axisYR` is the physical Y axis.
- Up/down/left/right sign.

Based on stockOS/spruceOS setting storage and hardware event results, initial
plumOS treats the left-stick click as unconnected/unsupported.

These will be confirmed with separate direction captures. Until then,
`--x-source`, `--y-source`, `--invert-x`, and `--invert-y` are available for
adjustment.

## Device Result On 2026-06-06

Read and normalize:

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

`plumos-joystickd` can create a virtual input device through `/dev/uinput` on
the A30. The device is axes-only; the provisional `BTN_THUMBL` was removed
because the left-stick click is treated as unsupported.

## Linux Joystick API / evdev Check

`plumos-joystick-reader` was confirmed on the A30 to read the `js0`/`event4`
device created by the daemon.

Open/init result without moving the stick:

```text
plumOS joystick reader
js=/dev/input/js0 event=/dev/input/event4 name="plumOS A30 Analog Stick" timeout_ms=1000 no_js=no no_event=no
js path=/dev/input/js0 open=yes
evdev path=/dev/input/event4 open=yes
js event index=1 time=328840 type=0x82 type_name=JS_AXIS_INIT number=0 value=0
js event index=2 time=328840 type=0x82 type_name=JS_AXIS_INIT number=1 value=-669
summary js_events=2 evdev_events=0 timeout_ms=1000
```

During a 10-second hardware test with stick movement, events were observed from
both the Linux joystick API and evdev:

```text
summary js_events=370 evdev_events=759 timeout_ms=10000
```

The device created by `plumos-joystickd` can therefore be treated as visible
through the Linux joystick API and evdev. Next, verify RetroArch/SDL recognition.

The left-stick click was not observed with `plumos-input-compare --all-events`
or `/dev/ttyS0` serial frames, and no saved stockOS/spruceOS setting for it was
observed. Initial plumOS therefore exposes only X/Y axes from the analog stick
daemon.

## Options

- `--serial PATH`: serial raw stick path. Default: `/dev/ttyS0`.
- `--calibration PATH`: calibration file. Default: `/config/joypad.config`.
- `--uinput PATH`: uinput path. Default: `/dev/uinput`.
- `--timeout-ms MS`: stop after this long. Default `0` runs until signaled.
- `--no-uinput`: read and normalize without creating a virtual input device.
- `--x-source NAME`: `axisYL`, `axisXL`, `axisYR`, or `axisXR`.
- `--y-source NAME`: `axisYL`, `axisXL`, `axisYR`, or `axisXR`.
- `--invert-x`, `--invert-y`: invert normalized output.
- `--deadzone-raw N`: raw deadzone around center. Default: `8`.
- `--print-every N`: print raw/normalized values every N frames.

## joystick-reader Options

- `--js PATH`: joystick API path. Default: `/dev/input/js0`.
- `--event PATH`: evdev path. If omitted, auto-detect by device name and fall
  back to `/dev/input/event4`.
- `--name NAME`: device name for evdev auto-detection. Default:
  `plumOS A30 Analog Stick`.
- `--timeout-ms MS`: monitor duration.
- `--no-js`, `--no-event`: check only one side.

## Direction

- The frontend should keep reading button events directly from `/dev/input/event3`
  for now.
- Treat `plumos-joystickd` as the analog-stick path for RetroArch and standalone
  emulators.
- Keep `plumos-joystickd` axes-only with `ABS_X`/`ABS_Y`; do not expose a stick
  click button.
- Consider enabling the daemon only while an emulator is running.
- Check SDL/RetroArch after `plumos-joystick-reader` confirms the Linux joystick
  API/evdev foundation on hardware.
- Eventually, launch profiles should declare whether they need the analog daemon.
