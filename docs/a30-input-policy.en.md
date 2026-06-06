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

## Policy

- Keep stock `keymon` for the initial frontend work.
- Implement plumOS frontend controls by reading `/dev/input/event3` directly.
- Do not use exclusive mechanisms such as `EVIOCGRAB` while coexisting with
  stock MainUI.
- Confirm button code/action mapping with
  `plumos-input-compare --timeout-ms 10000` while pressing physical buttons.
- Revisit whether to keep or stop `keymon` when plumOS becomes the regular
  boot frontend.

Current recommendation: keep `keymon`, but let the plumOS frontend read input
events directly.
