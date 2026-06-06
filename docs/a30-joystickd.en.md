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

Create a PPSSPP-style composite gamepad briefly:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --device-mode xbox --timeout-ms 5000'
```

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-joystickd-xbox.sh --deploy
```

This mode combines the `/dev/ttyS0` left-stick axes and physical buttons from
`/dev/input/event3` into one virtual gamepad. It is the leading candidate for an
always-running plumOS input service, but duplicate input across the frontend,
`keymon`, and emulators must be checked before making it the default.

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

## RetroArch / SDL Check

Stock RetroArch 1.16.0 reports these build features:

```text
SDL             - SDL input/audio/video drivers: yes
SDL2            - SDL2 input/audio/video drivers: no
UDEV            - UDEV/EVDEV input driver: no
```

Stock `retroarch.cfg` uses `input_driver = "sdl"` and
`input_joypad_driver = "sdl"`, so the stock path to check is SDL1 joystick
input.

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --deploy
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --device-mode analog
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-joystick.sh --device-mode xbox
```

Result observed on 2026-06-06:

```text
js info path=/dev/input/js0 name="plumOS A30 Analog Stick" axes=2 buttons=0
evdev info path=/dev/input/event4 name="plumOS A30 Analog Stick"
[INFO] [Input]: Found input driver: "sdl".
[INFO] [Joypad]: Found joypad driver: "sdl".
result=retroarch_sdl_driver_only
```

Temporary `sdl` and `sdl_dingux` autoconfig files were tested, but the RetroArch
log did not show configured/connection lines for `plumOS A30 Analog Stick`.

The same script was extended with `--device-mode xbox` and re-run with
`plumOS A30 Gamepad`.

```text
detected mode=xbox name="plumOS A30 Gamepad" js=/dev/input/js0 event=/dev/input/event4
js info path=/dev/input/js0 name="plumOS A30 Gamepad" axes=8 buttons=11
evdev info path=/dev/input/event4 name="plumOS A30 Gamepad"
[INFO] [Input]: Found input driver: "sdl".
[INFO] [Joypad]: Found joypad driver: "sdl".
result=retroarch_sdl_driver_only
```

Stock RetroArch stdout prints `joystick read cal` and `thread serial joystick`,
but RetroArch logs still do not show configured/connection lines for either the
axes-only or composite `plumOS A30 ...` device.

In a manual stock RetroArch launch from MainUI, the Port 1 Controls binding UI
detects the left stick as `Axis -2`/`+/-2`, but moving the stick does not move
the menu cursor. From SSH, the stock RetroArch process mapped `libSDL-1.2.so.0`,
but its open input fd was only `/dev/input/event0`; it did not open
`/dev/input/js*` or `event4`.

Current judgment:

- The `plumos-joystickd` virtual device is visible through the Linux joystick
  API and evdev.
- The stock RetroArch SDL1 joypad driver is enabled, but logs do not confirm
  autoconfig/connection for either the axes-only `js0` device or the composite
  `plumOS A30 Gamepad`.
- Stock RetroArch appears to detect an axis during binding, but that does not
  translate into normal menu/game operation.
- The RetroArch analog strategy should not rely on this stock SDL1 behavior.
  Prioritize a plumOS RetroArch build with SDL2/evdev support plus the
  buttons+axes composite virtual pad.

## PPSSPP / SDL2 GameController Check

This was checked while stock PPSSPP was launched from MainUI and the left stick
was working in PPSSPP.

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-input.sh
```

`/mnt/SDCARD/Emu/PPSSPP/launch.sh` starts a dedicated input daemon before
starting PPSSPP.

```sh
./miyoo282_xpad_inputd&
./PPSSPPSDL "$*"
killall miyoo282_xpad_inputd
```

Processes observed while PPSSPP was running:

```text
/mnt/SDCARD/Emu/PPSSPP/launch.sh
./miyoo282_xpad_inputd
./PPSSPPSDL /mnt/SDCARD/Emu/PPSSPP/../../Roms/PSP/Puzzle_Bobble.cso
```

The following virtual pad appears in `/proc/bus/input/devices`:

```text
I: Bus=0003 Vendor=045e Product=028e Version=045e
N: Name="MIYOO Pad1"
S: Sysfs=/devices/virtual/input/input18
H: Handlers=js0 event4
B: EV=2b
B: KEY=7cdb0000 0 0 0 0 0 0 0 0 0
B: ABS=3003f
```

Through `plumos-joystick-reader`, `MIYOO Pad1` appears as an 8-axis /
11-button composite device.

```text
js info path=/dev/input/js0 name="MIYOO Pad1" axes=8 buttons=11
evdev info path=/dev/input/event4 name="MIYOO Pad1"
```

Observed process fds:

```text
miyoo282_xpad_inputd -> /dev/uinput
miyoo282_xpad_inputd -> /dev/ttyS0
PPSSPPSDL -> /dev/input/event4
PPSSPPSDL -> /dev/ttyS0
```

`miyoo282_xpad_inputd` contains strings for:

```text
/config/joypad.config
/dev/ttyS0
MIYOO Pad1
/dev/uinput
```

`PPSSPPSDL` uses `libSDL2-2.0.so.0` and `SDL_GameController*` /
`SDL_Joystick*` APIs. Embedded paths indicate a PPSSPP 1.16.6-based build.
`assets/gamecontrollerdb.txt` contains Xbox 360 controller mappings, and
`MIYOO Pad1`'s `045e:028e` identity is likely routed through that family of
mappings.

Current judgment:

- PPSSPP analog stick support works because the launch script starts
  `miyoo282_xpad_inputd`, which creates a composite virtual pad, not because
  PPSSPP itself directly handles the raw serial stick protocol.
- `miyoo282_xpad_inputd` reads `/dev/ttyS0` and `/config/joypad.config`, then
  creates an Xbox 360-like `MIYOO Pad1` through `/dev/uinput`.
- PPSSPP reads `MIYOO Pad1` from `/dev/input/event4` through SDL2
  GameController/Joystick APIs.
- The left-stick click did not react in PPSSPP controller settings, so keep
  treating it as unconnected/unsupported.
- plumOS should not reuse the stock binary, but this strongly supports adding a
  buttons+axes composite virtual pad mode to `plumos-joystickd`.
- The plumOS RetroArch path should prioritize SDL2/evdev plus a composite
  virtual pad instead of relying on the stock SDL1 path.

## Composite Gamepad Mode

As of 2026-06-06, `plumos-joystickd` has a `--device-mode xbox` mode. This does
not reuse stock `miyoo282_xpad_inputd`; it implements the same principle on the
plumOS side based on the PPSSPP observations.

Created device:

```text
name="plumOS A30 Gamepad"
id=045e:028e
axes=ABS_X,ABS_Y,ABS_Z,ABS_RX,ABS_RY,ABS_RZ,ABS_HAT0X,ABS_HAT0Y
buttons=BTN_A,BTN_B,BTN_X,BTN_Y,BTN_TL,BTN_TR,BTN_SELECT,BTN_START,BTN_MODE,BTN_THUMBL,BTN_THUMBR
```

Mapping:

| source | virtual output |
| --- | --- |
| `/dev/ttyS0` X/Y | `ABS_X` / `ABS_Y` |
| D-pad | `ABS_HAT0X` / `ABS_HAT0Y` |
| A/B/X/Y | `BTN_A` / `BTN_B` / `BTN_X` / `BTN_Y` |
| L/R | `BTN_TL` / `BTN_TR` |
| L2/R2 | `ABS_Z` / `ABS_RZ` digital trigger value |
| START/SELECT | `BTN_START` / `BTN_SELECT` |
| Function | `BTN_MODE` |
| left-stick click | unsupported; capability exists, but no event is emitted |

`--button-event PATH` selects the physical button input event source. The
default is `/dev/input/event3`. Use `--no-buttons` to forward only the left-stick
axes into the composite gamepad.

Always-running policy:

- Treat `--device-mode xbox` as the leading candidate for normal plumOS runtime.
- During stock MainUI/PPSSPP investigation, run it with `--timeout-ms` because it
  adds another input device.
- Let the plumOS frontend continue reading `/dev/input/event3` directly for its
  own UI, and prefer passing the composite gamepad to emulators.
- Before making it the default service, check duplicate input and stale fd
  behavior in the frontend, `keymon`, RetroArch, and standalone emulators.

Hardware result on 2026-06-06:

This was run while stock PPSSPP was active, so stock `MIYOO Pad1` occupied
`js0`/`event4`. Running
`plumos-joystickd --device-mode xbox --timeout-ms 5000` created the plumOS
device as `js1`/`event5`.

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

This confirms that `xbox` mode can create an 8-axis / 11-button virtual device
on A30 hardware with capabilities matching the family observed from PPSSPP's
`MIYOO Pad1`. Next, verify SDL2 GameController recognition and
RetroArch/standalone emulator behavior.

### PPSSPP Direct Launch Check

When PPSSPP is stopped, `plumOS A30 Gamepad` is created as `js0`/`event4`.
With that device active, stock `PPSSPPSDL` was launched directly without
starting stock `miyoo282_xpad_inputd`.

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-plumos-gamepad.sh
```

Observed result:

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

This confirms that `plumos-joystickd --device-mode xbox` is recognized through
stock PPSSPP's SDL2 GameController path and successfully loads a GameController
mapping. Both process fds and PPSSPP logs show PPSSPP using the plumOS gamepad.
After the PPSSPP direct-launch and stock RetroArch probes finished, no stale
`plumos-joystickd` process, `plumOS A30 Gamepad` device, or
`/dev/uinput`/`event4`/`ttyS0` fd remained.

### Button Forwarding Check

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-joystickd-buttons.sh
```

On 2026-06-06, with stock MainUI/keymon still running,
`plumos-joystickd --device-mode xbox` was started briefly and physical buttons
were confirmed to forward into `plumOS A30 Gamepad`.

```text
A      -> BTN_A
B      -> BTN_B
X      -> BTN_X
Y      -> BTN_Y
D-pad  -> ABS_HAT0X / ABS_HAT0Y
L/R    -> BTN_TL / BTN_TR
L2/R2  -> ABS_Z / ABS_RZ
START  -> BTN_START
SELECT -> BTN_SELECT
FUNC   -> BTN_MODE
```

Observed summary:

```text
js info path=/dev/input/js0 name="plumOS A30 Gamepad" axes=8 buttons=11
evdev info path=/dev/input/event4 name="plumOS A30 Gamepad"
summary frames=1538 emitted=1 button_events=30 last_x=0 last_y=0 duration_ms=23172
result=button_forwarding_observed
```

Function was also checked separately and appeared as
`EV_KEY code=316 key=BTN_MODE`; through the joystick API it was
`JS_BUTTON number=8`. After the probe finished, no `plumos-joystickd` process or
`plumOS A30 Gamepad` device remained.

## Options

- `--serial PATH`: serial raw stick path. Default: `/dev/ttyS0`.
- `--calibration PATH`: calibration file. Default: `/config/joypad.config`.
- `--uinput PATH`: uinput path. Default: `/dev/uinput`.
- `--device-mode MODE`: `analog` or `xbox`. Default: `analog`.
- `--button-event PATH`: physical button event source for `xbox` mode. Default:
  `/dev/input/event3`.
- `--no-buttons`: do not forward physical buttons in `xbox` mode.
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
- Treat `analog` mode as the `ABS_X`/`ABS_Y` axes-only device and `xbox` mode as
  the emulator-facing buttons+axes composite virtual pad.
- Do not emit a left-stick click event in initial plumOS.
- Consider enabling the daemon only while an emulator is running.
- Do not tune for stock RetroArch/SDL1; prioritize the plumOS RetroArch build
  with SDL2/evdev and the `xbox` mode composite pad.
- Eventually, launch profiles should declare whether they need the input daemon
  and which device mode to use.
