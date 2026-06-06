# A30 Runtime Probe

`plumos-runtime-probe` is a statically linked diagnostic binary for checking the
minimal video/input/audio interfaces before replacing the A30 frontend/runtime.

`plumos-runtime-probe` does not link against the stock SDL libraries; it checks
the Linux interfaces directly. The dynamic-link/runtime strategy for SDL2 is
validated separately with `plumos-sdl2-probe`.

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

SDL2 linked/GameController probe:

```sh
./scripts/docker-build.sh sdl2-runtime
./scripts/docker-build.sh sdl2-probe
```

Outputs:

```text
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe.bin
dist/plumos-sdl2-probe/plumos/lib/
dist/plumos-sdl2-probe/plumos/share/doc/plumos-sdl2-probe/
```

`plumos-sdl2-probe` runs with upstream SDL3 3.4.10 and sdl2-compat 2.32.68 plus
the required dynamic loader and shared libraries bundled under
`/mnt/SDCARD/plumos/lib`. Because the A30 SD card cannot create symlinks,
sonames are copied as regular files rather than symlinks.

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

SDL2 linked/GameController probe:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

This script starts `plumos-joystickd --device-mode xbox` briefly and checks
whether `plumOS A30 Gamepad` is visible through SDL2 Joystick/GameController
APIs. By default it does not set `SDL_JOYSTICK_DEVICE`, so SDL2 uses normal
auto-detection. Use `--force-js-device` to pass the detected `/dev/input/js*`
path through `SDL_JOYSTICK_DEVICE` for comparison.

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

SDL2 linked/GameController probe:

```text
plumOS SDL2 probe
compiled_sdl=2.32.68 linked_sdl=2.32.68 timeout_ms=3000 no_video=no
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_JOYSTICK_DEVICE=-
sdl init=yes current_video_driver=dummy current_audio_driver=-
window create=yes error=""
joysticks=1
device index=0 name="Xbox 360 Controller" guid=0300e4fb5e0400008e0200005e040000 is_controller=yes
controller open index=0 yes
controller info index=0 name="Xbox 360 Controller" attached=yes axes=6 buttons=11 hats=1
summary joysticks=1 controllers_open=1 joysticks_open=0 controller_events=1 joystick_events=1
result=sdl2_gamecontroller_visible
```

This confirms SDL initialization and window creation with
`SDL_VIDEODRIVER=dummy`. The real framebuffer/render backend is still a separate
next probe.

With `--force-js-device` and `SDL_JOYSTICK_DEVICE=/dev/input/js0`, SDL2 listed
the same virtual pad as two entries: `plumOS A30 Gamepad` and
`Xbox 360 Controller`. For normal operation, prefer the auto-detected path
without that environment variable.

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
- SDL2: the plumOS-bundled upstream SDL3 3.4.10 + sdl2-compat 2.32.68 plus
  bundled dynamic loader/shared libraries starts successfully and automatically
  recognizes the `plumos-joystickd --device-mode xbox` composite virtual pad as
  a GameController.
- SDL2 render: dummy-video window creation succeeds; the real
  framebuffer/render backend is still unvalidated.

The stock `keymon` comparison is split into
[A30 input policy](a30-input-policy.en.md). Next steps are the real SDL2 render
backend and the RetroArch SDL2/evdev build.
