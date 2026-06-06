# A30 Runtime Probe

`plumos-runtime-probe` is a statically linked diagnostic binary for checking the
minimal video/input/audio interfaces before replacing the A30 frontend/runtime.

It does not link against the stock SDL libraries. Until SDL2 is bundled under
plumOS, this probe checks the Linux interfaces directly.

## Build

```sh
./scripts/docker-build.sh runtime-probe
```

Outputs:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/bin/plumos-shm-watch
dist/plumos-runtime-probe/plumos/bin/plumos-serial-joy-probe
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

## Deploy/Run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

Read-oriented safe check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --input-ms 100 --no-audio'
```

Short framebuffer draw/restore plus audio-state check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'
```

`--allow-busy-audio` treats the stock MainUI holding the PCM device as an
observed state rather than a probe failure.

Read-only SysV shared memory watch for stock `keymon`/`MainUI`:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-shm-watch --timeout-ms 10000 --interval-ms 100 --max-bytes 128'
```

`plumos-shm-watch` is a helper for investigating paths that do not appear as
kernel input events, such as left stick calibration. It does not write to shared
memory.

Serial joystick raw-data check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-serial-joy-probe --timeout-ms 5000 --stats-only'
```

The spruceOS A30 joystick reader uses `/dev/ttyS2`. On the stock A30 observed on
2026-06-06, joystick frames were found on `/dev/ttyS0` instead. `/dev/ttyS2`
does not exist initially, and although `/proc/tty/drivers` reports `ttyS` minors
0-4, a temporary `c 250 2` node failed to open with `ENXIO`.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'rm -f /tmp/plumos-ttyS2; mknod /tmp/plumos-ttyS2 c 250 2; /mnt/SDCARD/plumos/bin/plumos-serial-joy-probe --port /tmp/plumos-ttyS2 --timeout-ms 10000 --stats-only; rm -f /tmp/plumos-ttyS2'
```

`plumos-serial-joy-probe` prints 9600/8N1 raw serial data and candidate 6-byte
frames in the likely spruceOS format: `ff axisYL axisXL axisYR axisXR fe`.
With `--stats-only`, it suppresses per-frame output and prints min/max/avg for
each axis.

## Options

- `--fb PATH`: framebuffer path. Default: `/dev/fb0`.
- `--input PATH`: input event path. Default: auto-detected `gpio-keys-polled`.
- `--dsp PATH`: OSS audio path. Default: `/dev/dsp`.
- `--draw-ms MS`: draw and restore a small framebuffer patch.
- `--input-ms MS`: poll input events for this long.
- `--audio-ms MS`: write a short OSS test tone.
- `--allow-busy-audio`: treat `EBUSY` on the audio device as success.
- `--no-video`, `--no-input`, `--no-audio`: skip individual probes.

`PLUMOS_FB`, `PLUMOS_INPUT_EVENT`, and `PLUMOS_DSP` can override paths through
the environment.

## Device Result On 2026-06-06

Read-oriented check:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32 line=1920
video draw=skipped
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio skipped
result=ok
```

Draw/audio-state check:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,0 bpp=32 line=1920
video draw=ok pixels=64x64 ms=80
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio path=/dev/dsp open=busy allowed=yes errno=16 Device or resource busy
result=ok
```

`offset` can appear as `0,0` or `0,640` depending on the stock MainUI
framebuffer/pan state.

Additional inspection showed that the stock `MainUI` holds
`/dev/snd/pcmC0D0p`. While the stock frontend is running, the probe cannot write
a test tone through `/dev/dsp`.

## Decision

- Video: `/dev/fb0` reports 480x640, 32bpp, line length 1920, and short
  draw/restore succeeds.
- Input: `gpio-keys-polled` opens and polls as `/dev/input/event3`.
- Shared memory: `plumos-shm-watch` can attach to stock `keymon`/`MainUI` SysV
  shm read-only.
- Serial joystick: the `ttyS` driver supports minors 0-4, but the initial
  `/dev` only contains `/dev/ttyS0` and `/dev/ttyS1`. `/dev/ttyS0` emits
  joystick frames at 9600/8N1.
- Audio: OSS `/dev/dsp` exists, but is busy while stock MainUI holds PCM.
- SDL2: stock libraries exist, but are not adopted as plumOS runtime
  dependencies. Build a linked probe after plumOS bundles its own SDL2 package.

The stock `keymon` comparison is split into
[A30 input policy](a30-input-policy.en.md). Next steps are building a
plumOS-bundled SDL2 runtime or confirming the physical button mapping.
