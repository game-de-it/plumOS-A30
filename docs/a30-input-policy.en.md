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

Notes:

- The START menu opens from physical START (`KEY_ENTER`).
- Function (`KEY_ESC`) is not treated as an alternate START button. It is the
  primary candidate for a safe shutdown/resume menu.
- X/Y/L/R/L2/R2/volume buttons are identified by the probe but are not assigned
  to normal controller UI actions yet.
- The power button remains unconfirmed to avoid stock or kernel-side
  sleep/shutdown side effects. Design plumOS auto-resume without depending on
  the power button, with Function as the preferred fallback trigger.

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

Current recommendation: keep `keymon`, but let the plumOS frontend read input
events directly.
