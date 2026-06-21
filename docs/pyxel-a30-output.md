# Pyxel A30 Output Path

Investigation date: 2026-06-14

This document records how Pyxel is made practical on plumOS A30: display,
audio, input, FE launch behavior, and the relationship between the stock SDL
path, plumOS SDL runtime, and the optional patched Pyxel fallback.

Japanese counterpart: [pyxel-a30-output.ja.md](pyxel-a30-output.ja.md)

## Goal

The goal was to decide whether Pyxel can be treated as a practical plumOS app
runtime, whether Pyxel's expected output path can work on the current plumOS
runtime, and whether it can use the A30 Mali/GLES hardware path.

## What Pyxel Expects

Pyxel 2.9.6 calls its Rust implementation through `pyxel_binding.abi3.so`. On
Linux, `pyxel/__init__.py` first tries to load `libSDL2-2.0.so.0` with
`RTLD_GLOBAL`; only if that fails does it load the SDL2 bundled in the Python
package.

The normal Pyxel display path is SDL2 + OpenGL/GLES, not an SDL software
renderer:

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)`
- `SDL_CreateWindow(..., SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)`
- `SDL_GL_CreateContext`
  - try OpenGL 2.1
  - fall back to OpenGL ES 2.0
- create a `glow::Context` from `SDL_GL_GetProcAddress`
- upload Pyxel's 16-color indexed screen to a GL texture
- render through palette texture and shader
- present with `SDL_GL_SwapWindow`

The minimum plumOS requirement is therefore an SDL2 API surface that can create
a GLES2 context and swap buffers. This can be a hardware path for Pyxel, even
though Pyxel drawing APIs still render to a CPU-side canvas; the GPU mainly
handles texture upload, scaling, shaders, and presentation.

## Plain plumOS SDL2 Result

The normal `/mnt/SDCARD/plumos/bin/python3` wrapper originally fixed the library
path to:

```text
/mnt/SDCARD/plumos/python/lib:/mnt/SDCARD/plumos/lib:/usr/lib:/lib
```

That made Pyxel load plumOS `libSDL2-2.0.so.0` first. The visible drivers in
that older compatibility layer were:

```text
video: offscreen, dummy, evdev
audio: disk, dummy
```

`pyxel.init(..., headless=False)` failed with one of:

- automatic driver selection: `No available video device`
- `SDL_VIDEODRIVER=dummy`: no OpenGL window
- `SDL_VIDEODRIVER=offscreen`: no OpenGL window
- `SDL_VIDEODRIVER=evdev`: no OpenGL window

That runtime could not support Pyxel's normal output path.

## Stock SDL2 Mali Probe

As an experiment, the loader was called directly and the stock SDL2 path was
placed before plumOS SDL2:

```sh
/mnt/SDCARD/plumos/lib/ld-linux-armhf.so.3 \
  --library-path /mnt/SDCARD/plumos/python/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/plumos/lib:/usr/lib:/lib \
  --argv0 /mnt/SDCARD/plumos/bin/python3 \
  /mnt/SDCARD/plumos/python/bin/python3.14 pyxel_stock_sdl_probe.py
```

Stock SDL2 exposed:

```text
video: mali, offscreen
audio: alsa, dsp, disk, dummy
```

The hardware log confirmed the stock `mali` path:

```text
current_video_driver mali
/usr/lib/libMali.so mapped
current_mode 480x640
MALI_CreateWindow:0x20000001 done.
```

That proved Pyxel can draw through SDL2/GLES on the A30. However the raw stock
path uses the A30 portrait surface and does not match the plumOS landscape
frontend convention, so it is not the final route.

## Adopted Implementation

The adopted route is a clean-room plumOS SDL runtime path:

- plumOS SDL2 compatibility layer uses SDL3 underneath.
- SDL3 has a custom `a30mali` video backend.
- The backend opens `/dev/fb0` and `/dev/mali`.
- EGL/GLES2 comes from the A30 rootfs libraries such as `/usr/lib/libEGL.so` and
  `/usr/lib/libGLESv2.so`.
- Stock SDL2 binaries are not used by the normal Pyxel launcher.

The first-pass implementation added:

- `docker/plumos-toolchain/patches/sdl3-3.4.10-a30mali-video-driver.patch`
- `docker/plumos-toolchain/patches/pyxel-2.9.6-a30-fbo-present.patch`
  for fallback only
- `docker/plumos-toolchain/pyxel-a30-shim/plumos_pyxel_a30_shim.py`
- `docker/plumos-toolchain/scripts/build-sdl2-runtime.sh` patch application
- `docker/plumos-toolchain/scripts/build-pyxel-a30.sh`
- `scripts/docker-build.sh pyxel-a30`

Build:

```sh
scripts/docker-build.sh pyxel-a30
```

Artifact:

```text
dist/plumos-pyxel-a30/
```

Deployment adds:

```text
/mnt/SDCARD/plumos/experiments/pyxel-a30-site/
/mnt/SDCARD/plumos/bin/plumos-pyxel-a30
/mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch
/mnt/SDCARD/plumos/share/pyxel-a30/plumos_pyxel_a30_shim.py
/mnt/SDCARD/plumos/share/doc/plumos-pyxel-a30/manifest.txt
```

## Default Runtime Environment

`plumos-pyxel-a30` is the Python entry point for Pyxel. Defaults:

```text
HOME=/mnt/SDCARD
SDL_VIDEODRIVER=a30mali
SDL_AUDIODRIVER=alsa
SDL_OPENGL_LIBRARY=/usr/lib/libGLESv2.so
SDL_EGL_LIBRARY=/usr/lib/libEGL.so
PLUMOS_A30MALI_ROTATION=cw
SDL_GAMECONTROLLERCONFIG=plumOS A30 Gamepad mapping
```

`HOME` defaults to `/mnt/SDCARD` so Pyxel app user data does not escape into the
rootfs. Override only with `PLUMOS_PYXEL_HOME` when necessary.

The normal path does not prepend `/mnt/SDCARD/plumos/experiments/pyxel-a30-site`
to `PYTHONPATH`. It uses the Pyxel package installed under
`/mnt/SDCARD/plumos/python/lib/python3.14/site-packages`, so future
`pip install --upgrade pyxel` can keep working without a Pyxel fork.

`plumos-pyxel-a30` runs through `plumos_pyxel_a30_shim`. The shim wraps only the
public `pyxel.init()` API, uses `width`, `height`, and `display_scale` to set
`PLUMOS_A30MALI_LOGICAL_SIZE`, and then lets SDL `a30mali` keep aspect ratio and
rotation. For `.pyxapp`, it runs from the extracted startup script's parent
directory so relative resources such as `fonts/...` and `assets.pyxres` resolve
from the app directory.

## Rotation and Aspect Handling

With `PLUMOS_A30MALI_ROTATION=cw`, the standard Pyxel GLES backbuffer is copied
to a texture at `SDL_GL_SwapWindow` time. The presenter switches to the physical
framebuffer EGL surface, returns to the default framebuffer, draws a rotated quad
on the GPU, preserves aspect ratio, and swaps.

The source size prefers `PLUMOS_A30MALI_LOGICAL_SIZE`, which the shim derives
from Pyxel initialization. This is why a 720x480 Pyxel app such as
`LastEmulator.pyxapp` can fit into the A30 640x480 landscape visible area
without patching Pyxel itself.

`PLUMOS_A30MALI_SHADER_FIT` is enabled by default. Disable only by setting it to
`0`, `false`, or `off`.

## Fallback Patched Pyxel

The old A30-specific patched Pyxel presenter remains as a fallback in:

```text
/mnt/SDCARD/plumos/experiments/pyxel-a30-site/
```

It is not used by default. Enable it explicitly:

```sh
PLUMOS_PYXEL_USE_PATCHED=1 plumos-pyxel-a30-launch -m pyxel play <path>
```

When enabled, `plumos-pyxel-a30` prepends that site to `PYTHONPATH` and sets
`PLUMOS_PYXEL_A30_PRESENT=1` and `PLUMOS_PYXEL_A30_ROTATION=cw`.

## FE Launch

`systems.json` contains the `pyxel` system:

- ROM directory: `/mnt/SDCARD/Roms/pyxel`
- extensions: `.pyxapp`, `.py`
- launch profile: `pyxel:a30`

Dispatch:

- `.pyxapp`: `plumos-pyxel-a30-launch -m pyxel play <path>`
- `.py`: `plumos-pyxel-a30-launch -m pyxel run <path>`

## Launcher Responsibilities

`plumos-pyxel-a30-launch` handles:

- CPU policy apply/restore via `PLUMOS_PYXEL_CPU_POLICY`,
  `PLUMOS_PYXEL_CPU_FREQ`, and `PLUMOS_PYXEL_CPU_CORES`
- `plumos-volume-control apply` and `persist-runtime`
- `plumos-joystickd --device-mode xbox --trigger-mode buttons --shoulder-layout user`
- `plumos-safe-hotkeyd`
- cleanup of joystickd, safe-hotkeyd, and runtime state on exit

When the FE Core menu stores Pyxel CPU frequency/core overrides,
`plumos-text-ui launch` prefixes the command with variables such as:

```text
PLUMOS_PYXEL_CPU_POLICY=fixed
PLUMOS_PYXEL_CPU_FREQ=1344000
PLUMOS_PYXEL_CPU_CORES=4
```

The launcher logs applied `online`, governor, and frequency state under:

```text
/mnt/SDCARD/plumos/logs/pyxel/<run>-cpu.log
```

## Input Mapping

The Pyxel launcher overrides SDL GameController mapping for the
`plumos-joystickd` virtual pad:

```text
A/B/X/Y: b0/b1/b2/b3
L/R: b4/b5
L2/R2: b6/b7 -> lefttrigger/righttrigger
SELECT/START/FUNCTION: back/start/guide
D-pad: hat0
left stick: a0/a1
```

Use `--shoulder-layout user` for Pyxel. With `--shoulder-layout standard`, A30
physical L/R and L2/R2 appear swapped on the Pyxel side.

Pyxel analog keys must be read with `btnv()`. Using `btnp()` / `btnr()` on axis
keys can panic.

## Verified Behavior

SDL runtime probe:

```text
compiled_sdl=2.32.68 linked_sdl=2.32.68
video_driver index=0 name="a30mali"
sdl init=yes current_video_driver=a30mali
video_display index=0 name="1" bounds=0,0 480x640
gl vendor="ARM" renderer="Mali-400 MP"
gl swap=yes
```

Pyxel validation probe:

```text
env SDL_VIDEODRIVER=a30mali
env SDL_AUDIODRIVER=alsa
plumOS Pyxel A30 present enabled logical=640x480 physical=480x640 rotation=cw
summary events=0 frames=119 seconds=5.01
```

Important captures:

```text
artifacts/pyxel-demo/a30-present-captures/20260614-085325.visible.cw.png
artifacts/pyxel-demo/a30mali-captures/20260614-115752.visible.cw.png
artifacts/pyxel-demo/a30mali-lastemulator-captures/20260614-120249.visible.cw.png
artifacts/pyxel-demo/shim-fit-probe2/20260614-155501.visible.cw.png
artifacts/pyxel-demo/shim-lastemulator-final/20260614-155612.visible.cw.png
```

`LastEmulator.pyxapp` initializes at its native `720x480` logical size and can
take about 45 seconds before title capture is meaningful.

The SDL3 shader-fit path and the old patched Pyxel reference matched layout for
the LastEmulator Prologue screen:

```text
SDL3 shader-fit:
  artifacts/pyxel-demo/lastemulator-sdl3-shaderfit-prologue/20260614-175335.visible.cw.png
  visible bbox: 640x426+0+27

patched Pyxel reference:
  artifacts/pyxel-demo/trace-patched/20260614-151506.visible.cw.png
  visible bbox: 640x426+0+27

old failed FBO path:
  artifacts/pyxel-demo/lastemulator-fbo-fresh-prologue/20260614-164718.visible.cw.png
  visible bbox: 570x426+35+27
```

## Audio, Input, and Hotkey Checks

Validated with:

```text
/mnt/SDCARD/plumos/demos/pyxel_validation_probe.py
/mnt/SDCARD/plumos/demos/pyxel_guided_mapping_probe.py
```

Confirmed:

- Pyxel opens `/dev/snd/pcmC0D0p`, `/dev/snd/controlC0`, and `/dev/snd/timer`
  through `SDL_AUDIODRIVER=alsa`.
- Pyxel opens `/dev/fb0`, `/dev/mali`, and `/dev/input/event4`.
- `plumos-joystickd --device-mode xbox` exposes `plumOS A30 Gamepad` as
  `/dev/input/js0` / `/dev/input/event4`.
- `plumos-safe-hotkeyd` handles volume keys on `/dev/input/event3` and power key
  overlay handling on `/dev/input/event0` while Pyxel runs.
- No Pyxel, joystickd, or safe-hotkeyd process remains after wrapper exit.
- Runtime volume is saved through `plumos-volume-control persist-runtime`.

Guided mapping result:

```text
physical A        -> Pyxel A
physical B        -> Pyxel B
physical X        -> Pyxel X
physical Y        -> Pyxel Y
physical D-pad    -> Pyxel UP/DOWN/LEFT/RIGHT
physical START    -> Pyxel START
physical SELECT   -> Pyxel BACK
physical FUNCTION -> Pyxel GUIDE
physical L        -> Pyxel L
physical R        -> Pyxel R
physical L2       -> Pyxel LTRIG
physical R2       -> Pyxel RTRIG
left stick        -> Pyxel LX/LY
```

## Conclusion

- plumOS SDL runtime now has an `a30mali` backend, so SDL2/GLES apps can use
  `/dev/fb0` + Mali EGL without loading the stock SDL2 binary.
- Pyxel's normal output path can run on the A30 Mali hardware route.
- The default Pyxel path uses the standard pip-installed Pyxel package through
  sdl2-compat -> SDL3 `a30mali`, avoiding a Pyxel fork.
- The patched Pyxel FBO presenter remains only as an explicit fallback.
- Audio, input, volume hotkey handling, power overlay support, and cleanup work
  through the plumOS SDL `a30mali` path.
- The current `a30mali` backend focuses on SDL window + GLES context + swap.
  Apps requiring broader SDL_Renderer or window-management behavior may need
  additional backend work.
