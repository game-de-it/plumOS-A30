# A30 Input Policy

This document records the first comparison between stock `keymon` and direct
input-event reading from plumOS.

Conclusion: keep stock `keymon` for now. Direct reading from `/dev/input/event3`
works non-exclusively, so the plumOS frontend can continue using direct event
reading.

## Diagnostic Binary

`plumos-input-compare` is included in the `plumos-runtime-probe` package.

Build:

```sh
./scripts/docker-build.sh runtime-probe
```

Deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

Run:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

To capture the physical button mapping, run it for longer and press buttons on
the A30.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 10000'
```

Use `--all-events` when every discovered input event device should be polled at
the same time.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --all-events --timeout-ms 30000'
```

## Device Result On 2026-06-06

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

Observations:

- `gpio-keys-polled` is `/dev/input/event3`.
- Stock `keymon` opens `event0`, `event1`, `event2`, and `event3`.
- Stock `MainUI` also opens `event0`, `event1`, and `event3`.
- Even in that state, plumOS can open and poll `/dev/input/event3` directly.
- The 100ms run had no physical button press, so it recorded `events=0`.
- `pid` values are observational and change across boots.

## Physical Button Mapping

Captured on 2026-06-06 with stock `keymon` and stock `MainUI` still running,
using `plumos-input-compare --all-events`. All listed buttons were observed on
`/dev/input/event3`, `gpio-keys-polled`.

| Physical button | code | key name | probe action | frontend action |
| --- | ---: | --- | --- | --- |
| Up | 103 | `KEY_UP` | `up` | cursor up |
| Down | 108 | `KEY_DOWN` | `down` | cursor down |
| Left | 105 | `KEY_LEFT` | `left` | cursor left/back |
| Right | 106 | `KEY_RIGHT` | `right` | cursor right/open |
| A | 57 | `KEY_SPACE` | `a` | A/open |
| B | 29 | `KEY_LEFTCTRL` | `b` | B/back |
| X | 42 | `KEY_LEFTSHIFT` | `x` | reserved |
| Y | 56 | `KEY_LEFTALT` | `y` | reserved |
| L | 15 | `KEY_TAB` | `l` | reserved |
| R | 14 | `KEY_BACKSPACE` | `r` | reserved |
| L2 | 18 | `KEY_E` | `l2` | reserved |
| R2 | 20 | `KEY_T` | `r2` | reserved |
| Volume - | 114 | `KEY_VOLUMEDOWN` | `volume_down` | reserved |
| Volume + | 115 | `KEY_VOLUMEUP` | `volume_up` | reserved |
| Function | 1 | `KEY_ESC` | `function` | safe menu candidate |
| START | 28 | `KEY_ENTER` | `start` | START menu |
| SELECT | 97 | `KEY_RIGHTCTRL` | `select` | core menu |
| Left stick axes | - | - | - | not exposed through kernel input yet |
| Left stick click | - | - | - | not observed via kernel input/serial frame |

Notes:

- The START menu opens from physical START (`KEY_ENTER`).
- Function (`KEY_ESC`) is not treated as an alternate START button. It is the
  primary candidate for a safe shutdown/resume menu.
- X/Y/L/R/L2/R2/volume buttons are identified by the probe but are not assigned
  to normal controller UI actions yet.
- The power button remains unconfirmed to avoid stock or kernel-side
  sleep/shutdown side effects. Design plumOS auto-resume without depending on
  the power button, with Function as the preferred fallback trigger.

## Left Analog Stick

Additional checks on 2026-06-06 show that the left stick is not exposed as a
normal analog axis under `/dev/input/event*`.

What was checked:

- Added `EV_ABS` output to `plumos-input-compare --all-events`.
- `/proc/bus/input/devices` does not show a stick device with `ABS_X`/`ABS_Y`.
- Moving the left stick did not produce observed `EV_ABS` events.
- Stock `MainUI.stock` mmaps `/dev/mem` and contains strings for
  `JoypadCalibration`, `JoypadTest`, `ADC INFO`, and `/config/joypad.config`.
- Stock `keymon` also contains strings for `/dev/mem`, `ABS_X`, `ABS_Y`, and
  `BTN_THUMBL`.
- Running calibration from MainUI Settings updates `/config/joypad.config`.
- In the spruceOS A30 implementation, `joystickinput` reads raw data from
  `/dev/ttyS2`, applies `/config/joypad.config`, sends analog events to
  `/dev/input/event4`, and sends keyboard/D-pad events to `/dev/input/event3`.
- The stock A30 initially has no `/dev/ttyS2` node, but `/proc/tty/drivers`
  reports `ttyS` minors 0-4 as available.
- On the observed stock A30, `/dev/ttyS0` produced 9600/8N1 6-byte frames in the
  form `ff b1 b2 b3 b4 fe`.

Observed `/config/joypad.config` values:

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

Observed `plumos-serial-joy-probe --port /dev/ttyS0 --stats-only` values:

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

Left-stick click check:

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

Click setting storage check:

- StockOS `/config/joypad.config` only contains six fields: `x_min`, `x_max`,
  `y_min`, `y_max`, `x_zero`, and `y_zero`.
- StockOS `/config/system.json` has a normal-button `keymap`, but no observed
  left-stick-click entry.
- Stock `MainUI` strings include `JoypadCalibration`, `JoypadTest`,
  `/config/joypad.config`, and the six X/Y calibration keys, but no observed
  field for saving a stick click.
- Stock `keymon` strings include `BTN_THUMBL`/`BTN_THUMBR`, but this is likely
  from a generic Linux input key-name table and is weak evidence for actual
  hardware events or saved settings.
- spruceOS `run_analog_stick_calibration` also writes only X/Y min/max/center
  values to `joypad.config`; it does not sample or save a click.

Current inference:

- The left stick axes are probably not kernel input events. A userland daemon
  likely converts serial raw data into virtual input events.
- On the observed A30, `/dev/ttyS0` is the primary raw-data candidate rather
  than spruceOS's `/dev/ttyS2`. `axisYR`/`axisXR` closely match
  `/config/joypad.config` min/max values and are likely the actual left-stick
  X/Y axes.
- `/dev/mem` may still be used by stock `MainUI` for calibration/test screens or
  other hardware control, but it is no longer the primary suspected stick path.
- Calibration saves raw min/max/center values to `/config/joypad.config`.
- Because plumOS should not reuse stock/spruce binaries directly, a later step
  should design `plumos-joystickd` around the `/dev/ttyS0` raw-data path.
- The left-stick click does not appear in normal kernel input events or in the
  `/dev/ttyS0` 6-byte serial frame. It also does not appear in observed
  stockOS/spruceOS settings, so initial plumOS treats it as
  unconnected/unsupported.

## PPSSPP Analog Input Path

This was checked while stock PPSSPP was launched from MainUI and the left stick
was working inside PPSSPP.

Repeatable script:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-ppsspp-input.sh
```

Results:

- PPSSPP `launch.sh` starts `./miyoo282_xpad_inputd&` before running
  `./PPSSPPSDL "$*"`.
- `miyoo282_xpad_inputd` opens `/dev/uinput` and `/dev/ttyS0`.
- Strings in `miyoo282_xpad_inputd` include `/config/joypad.config`,
  `/dev/ttyS0`, `MIYOO Pad1`, and `/dev/uinput`.
- While PPSSPP is running, a virtual input device named `MIYOO Pad1` appears.
- `MIYOO Pad1` looks like an Xbox 360-style composite device:
  `045e:028e`, `js0`/`event4`, 8 axes / 11 buttons.
- `PPSSPPSDL` opens `/dev/input/event4` and uses `libSDL2-2.0.so.0` plus
  `SDL_GameController*` / `SDL_Joystick*` APIs.
- The left-stick click did not react in PPSSPP controller settings.

Judgment:

- For standalone emulators, a buttons+axes composite virtual pad is more
  promising than an axes-only device.
- Do not reuse stock `miyoo282_xpad_inputd`; implement the same principle as a
  `plumos-joystickd` mode.
- For RetroArch, prefer testing a plumOS SDL2/evdev build with a composite
  virtual pad over matching the stock SDL1 path.
- Keep treating the left-stick click as unsupported because PPSSPP does not see
  it either.

## Policy

- Keep stock `keymon` for the initial frontend work.
- Implement plumOS frontend controls by reading `/dev/input/event3` directly.
- Do not use exclusive mechanisms such as `EVIOCGRAB` while coexisting with
  stock MainUI.
- Button code/action mapping is confirmed for every button except power.
- Prefer opening the safe shutdown/resume menu from Function instead of the
  power key.
- Revisit whether to keep or stop `keymon` when plumOS becomes the regular
  boot frontend.
- Do not include the left-stick click in the initial mapping. Revisit only if
  new evidence appears.
- Prioritize a `plumos-joystickd` composite virtual pad mode for emulator-facing
  analog-stick support.
- Stock PPSSPP was launched directly without `miyoo282_xpad_inputd`, and
  `plumOS A30 Gamepad` was assigned to pad 1 after a successful SDL2
  GameController mapping.
- Under stock RetroArch/SDL1, both the axes-only and composite gamepad devices
  are visible through the Linux joystick API, but RetroArch logs do not confirm
  autoconfig/connection. Prefer SDL2/evdev plus the composite virtual pad in the
  plumOS RetroArch build.
- `plumos-joystickd --device-mode xbox` forwards Function as `BTN_MODE`, so it
  can be used as a candidate safe-menu input while an emulator is running.
- The plumOS-bundled upstream SDL3 3.4.10 + sdl2-compat 2.32.68 probe also
  confirms that `plumos-joystickd --device-mode xbox` is auto-detected as an
  SDL2 GameController.
- `scripts/probe-a30-joystickd-residency.sh` confirms that in the stockless
  plumOS state, `plumos-joystickd --device-mode xbox` can stay resident while
  the FE reads `/dev/input/event3` directly and the SDL2 probe plus PPSSPP
  direct launch recognize `plumOS A30 Gamepad` (`event4`) as a GameController.
- After that residency probe exits, no stale `plumos-joystickd` process,
  `plumOS A30 Gamepad` device, or `/dev/uinput`/`event4`/`ttyS0` fd remains.
- PPSSPP direct launch also opens `/dev/input/event3` and `/dev/ttyS0` in
  addition to the plumOS gamepad `event4`; physical duplicate-input or serial
  read contention still needs a targeted check.

Current recommendation: stop stock `keymon` during regular plumOS operation,
let the frontend read `/dev/input/event3` directly, and treat resident
`plumos-joystickd --device-mode xbox` as the leading emulator input path.
Default PPSSPP launch profiles should wait until the extra `event3`/`ttyS0`
opens are understood.
