# A30 Stock SDL Video Path

On 2026-06-07, the stockOS SDL libraries and key binaries were copied from the
device and inspected with `strings`, ELF dynamic sections, an on-device SDL2
probe, and `/proc` data from the running `MainUI.stock` process. Stock binaries
are reference material only. Reusing them in plumOS requires prior confirmation.

## Summary

- Stock SDL2 is `libSDL2-2.0.so.0.2600.1`; `SDL_GetVersion()` reports `2.26.1`.
- Stock SDL2 has two video drivers: `mali` and `offscreen`. It does not include
  the `dummy` video driver.
- With `SDL_VIDEODRIVER` unset, SDL2 selects `mali` and can create a 480x640
  display plus an `opengles2` renderer.
- The `mali` driver appears to be a Miyoo/Allwinner-specific custom backend
  added to SDL2. Its strings include `mali-fbdev`, `Mali EGL Video Driver`,
  `/dev/fb0`, and `MALI_CreateWindow`.
- `libSDL2-2.0.so.0` does not directly list `libEGL.so`, `libGLESv2.so`, or
  `libMali.so` in its dynamic section. SDL2 dynamically loads the EGL/GLES
  libraries.
- On the rootfs, `/usr/lib/libEGL.so` and `/usr/lib/libGLESv2.so` ultimately
  point to `/usr/lib/libMali.so`.
- The running stock `MainUI.stock` maps the stock SDL2 libraries and
  `/usr/lib/libMali.so`, and has open fds for `/dev/mali` and `/dev/fb0`.
- Stock SDL1 contains `fbcon` and `dummy` video drivers and includes fbdev
  strings such as `/dev/fb0`, `FBIOPAN_DISPLAY`, `FBIOPUT_VSCREENINFO`, and
  `SDL_VIDEO_FBCON_ROTATION`.

## Stock SDL2 Probe

The existing `plumos-sdl2-probe.bin` was run with the stock SDL2 library placed
first in the library path. Because the probe binary requires a newer glibc, the
toolchain armhf glibc set was temporarily unpacked to
`/tmp/plumos-stock-sdl-glibc`.

Representative output:

```text
compiled_sdl=2.32.68 linked_sdl=2.26.1
video_drivers=2
video_driver index=0 name="mali"
video_driver index=1 name="offscreen"
current_mode 480x640
sdl init=yes current_video_driver=mali
video_display index=0 name="0" bounds=0,0 480x640
render_driver index=0 name="opengles2"
render_driver index=1 name="opengles"
render_driver index=2 name="software"
MALI_CreateWindow:0x20000001 done.
render info name="opengles2" flags=0xa max_texture=4096x4096
render present=yes readpixels=yes
```

`SDL_VIDEODRIVER=dummy` fails:

```text
sdl init=no error="dummy not available"
```

`SDL_VIDEODRIVER=offscreen` succeeds with the software renderer, but it is not a
real display path.

## Build Clues

Stock SDL2 contains this build path:

```text
/home/alfchen/ProjectData/miyoo_allwinner/a33_tina3.5.1/out/r16-parrot/compile_dir/target/libsdl2/src/render/opengles2/SDL_render_gles2.c
```

This suggests a Miyoo/Allwinner Tina 3.5.1 build tree with a custom `mali` video
backend added to upstream SDL2 2.26.1. The dynamic section only lists common
libc-side dependencies, so EGL/GLES is dynamically loaded:

```text
NEEDED libm.so.6
NEEDED libdl.so.2
NEEDED libpthread.so.0
NEEDED librt.so.1
NEEDED libgcc_s.so.1
NEEDED libc.so.6
SONAME libSDL2-2.0.so.0
```

The rootfs GPU library layout is:

```text
/usr/lib/libEGL.so -> libEGL.so.1 -> libEGL.so.1.4 -> libMali.so
/usr/lib/libGLESv1_CM.so -> ... -> libMali.so
/usr/lib/libGLESv2.so -> libGLESv2.so.2 -> libGLESv2.so.2.0 -> libMali.so
/usr/lib/libMali.so
/usr/lib/libUMP.so.3.0.0
```

`libMali.so` contains `__egl_platform_*_fbdev`, `/dev/fb0`, `/dev/mali`, and
`MALI_FBDEV` strings, confirming that it includes an fbdev EGL platform.

[weimingtom/miyoo_a30_playground](https://github.com/weimingtom/miyoo_a30_playground)
is also useful external reference material for A30 SDL work. Its README mentions
`--enable-video-a30`, `libEGL.so`/`libGLESv2.so`, and
`src/video/a30/SDL_a30_video.c`. The `sdltest/sdl-main.tar.gz` archive contains
an SDL1 A30 backend that uses `eglGetDisplay(EGL_DEFAULT_DISPLAY)` and
`eglCreateWindowSurface(..., 0, ...)`, matching the clean-room probe's
successful `NULL` native-window path below.

## Per-App Usage

`MainUI.stock`:

- Links against `libSDL2-2.0.so.0`, `libSDL2_ttf`, `libSDL2_image`, and
  `libSDL2_mixer`.
- Contains `SDL_CreateWindow`, `SDL_CreateRenderer`, and `SDL_RenderPresent`
  strings.
- Maps `/mnt/SDCARD/miyoo/lib/libSDL2-2.0.so.0` and `/usr/lib/libMali.so` while
  running.
- Opens `/dev/mali` and `/dev/fb0`.

`sdlloading`:

- Uses SDL1 and contains `SDL_SetVideoMode`, `SDL_Flip`, and `/dev/fb0`.
- The loading screen is probably using the SDL1 fbcon path.

`RetroArch`:

- `retroarch.cfg` sets `video_driver = "gl"`, `input_driver = "sdl"`, and
  `input_joypad_driver = "sdl"`.
- The binary links against `libSDL-1.2.so.0` and `libMali.so`, and contains
  `egl*`, `SDL_GL*`, `sdl_dingux`, `sunxi`, and `/dev/fb0` strings.
- Input is SDL1, but video is not just plain SDL1 fbcon; it also uses a
  GL/EGL/Mali path.

`PPSSPPSDL`:

- Links against `libSDL2-2.0.so.0`, `libMali.so`, and `libEGL.so`.
- It appears to combine SDL2 window/input with PPSSPP's own GLES/EGL renderer.
- `launch.sh` starts `miyoo282_xpad_inputd` before launching `PPSSPPSDL`.

## Implications For plumOS

The upstream SDL3+sdl2-compat runtime does not include the stock SDL2 `mali`
driver. As a first clean-room step, `plumos-mali-egl-probe` was implemented to
validate a minimal fbdev + Mali EGL presenter without linking to stock SDL.

```sh
./scripts/docker-build.sh mali-egl-probe
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 500 --frames 30
```

Device result:

```text
egl initialize=yes version=1.4
egl version="1.4 Linux-r8p1-00rel0"
egl surface mode=null create=yes native=(nil) surface=0x20000001
egl surface_size=480x640 mode_used=null
egl context es2=yes context=0x40000001
egl make_current=yes
gl renderer="Mali-400 MP"
draw frames=19 swap_ok=yes
gl readpixels rgba=381f96ff
result=mali_egl_present_ok
```

Both `NULL` native window and a `uint16_t width,height` fbdev window work;
`uint32_t width,height` fails with `EGL_BAD_NATIVE_WINDOW`. The surface handle
matches the stock SDL2 probe's `0x20000001`.

Real display output for plumOS therefore needs one of these paths:

- Build a clean-room fbdev + Mali EGL presenter based on the observed stock
  behavior.
- Add an A30 custom video backend to the SDL3/SDL2-compatible runtime.
- Start the frontend with a direct framebuffer presenter, while testing
  RetroArch and standalone emulators against their upstream EGL/GLES backends
  using the A30 `/usr/lib/libMali.so` stack.

Using stock binaries or source in the plumOS runtime requires prior approval.
