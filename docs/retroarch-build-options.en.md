# RetroArch Build Option Policy

Investigation date: 2026-06-07

This note records the observed Miyoo A30 stockOS RetroArch feature set and maps
it into a practical plumOS RetroArch build-option plan. It is not a table of
stockOS versions/options to copy. plumOS should prefer the upstream latest stable
available at build time, and only compare against stockOS when A30 testing shows
a regression, incompatibility, or hardware-specific problem.

## Source

- RetroArch upstream: <https://github.com/libretro/RetroArch>
- FCEUmm upstream: <https://github.com/libretro/libretro-fceumm>
- Gambatte upstream: <https://github.com/libretro/gambatte-libretro>
- Core info upstream: <https://github.com/libretro/libretro-core-info>

## StockOS RetroArch Feature

Observed on the A30 with `/mnt/SDCARD/RetroArch/retroarch --version`:

```text
RetroArch 1.16.0 Apr 25 2024
Compiler: GCC (7.5.0) 32-bit
MIYOO Build:Apr 25 2024 22:32:35
```

Main enabled features from `--features`:

```text
SDL yes
SDL2 no
Threads yes
OpenGL yes
OpenGLES yes
EGL yes
ALSA yes
OSS yes
7zip yes
zlib yes
rpng yes
rjpeg yes
Dynamic yes
Network Command yes
Netplay yes
LibretroDB yes
GLSL yes
```

Main disabled features:

```text
X11/Wayland/KMS/Vulkan/Metal no
UDEV no
FFmpeg no
FreeType no
SDL_image no
PulseAudio/Jack/OpenAL/OpenSL no
Libusb no
Qt/Cocoa no
Video4Linux2 no
```

## Keep Candidates For plumOS

| group | option / feature | reason |
| --- | --- | --- |
| core loading | `--enable-dylib` | Required to load libretro cores dynamically. |
| video | `--enable-opengles`, `--enable-egl`, `--enable-mali_fbdev` | A30 real-screen output is confirmed with `fbdev_mali` + GLES/EGL. |
| video | `--enable-rgui` | Lightweight and suitable for the A30 screen. Use as the first menu driver. |
| shader | GLSL enabled | The minimal build failed GL initialization when GLSL was disabled. |
| CPU | `--enable-neon`, `--enable-floathard`, `--enable-threads` | A30 is ARMv7 NEON/VFPv4 hard-float. |
| archive | `--enable-zlib`, `--enable-builtinzlib`, `--enable-7zip` | The NES/GB ROM set contains zip files, and compressed content is useful later. |
| remote control | `--enable-networking`, `--enable-command` | Candidate path for safe save/quit from the shutdown/resume flow. |
| audio | `--enable-oss` first, `--enable-alsa` if sysroot/deps are stable | Stock has both OSS and ALSA. Verify actual A30 device behavior. |
| input | `--enable-sdl2` and/or `--enable-udev` candidate | Avoid stock SDL1; plumOS prefers `plumos-joystickd --device-mode xbox` with SDL2/evdev. |
| content | `--enable-chd`, `--enable-flac` candidate | Needed if PS1/PCE CD/Mega CD/Neo Geo CD remain in the practical target set. |
| info | core info cache optional | Not required because the FE owns launch profiles, but useful for core metadata. |

## Defer Candidates

| group | feature | reason |
| --- | --- | --- |
| Netplay | Netplay | Stock enables it, but A30 Wi-Fi/CPU make it lower priority. Keep separate from Network Command. |
| LibretroDB | database/playlists | The FE manages ROM scan/metadata, so this is not required for the first full runtime. |
| screenshots/images | rpng/rjpeg/screenshots | Useful later for visual checks/thumbnails, but audio/input come first. |
| menu | Ozone/XMB/MaterialUI | Use RGUI on A30; polish belongs mostly in the FE. |
| rewind/runahead | rewind, runahead | Disable initially for A30 CPU/RAM headroom. |
| softpatch | patch/xdelta | Re-enable when IPS/BPS support is needed. |

## Disable Candidates

| group | feature | reason |
| --- | --- | --- |
| desktop/windowing | X11, Wayland, KMS, GLX, XVideo, Vulkan, Metal, Qt, Cocoa | Not part of the stockless plumOS real-screen path. |
| SDL1 | `--disable-sdl` | Do not depend on stock SDL1; align with plumOS input design. |
| heavy media | FFmpeg, V4L2, camera, microphone | Not needed for the initial emulator runtime. |
| desktop audio | PulseAudio, PipeWire, Jack, OpenAL, RSound, RoarAudio | Use OSS/ALSA first on the A30 rootfs. |
| HID extras | libusb, blissbox, parport, bluetooth | Prioritize builtin controls and joystickd. |
| online updater | update cores/assets/info, SSL, Discord | Keep updates in plumOS package management, not on-device RetroArch updates. |
| shader stack | Cg, HLSL, Slang, glslang, SPIRV-Cross | Mali-400/GLES2 only needs GLSL here. |
| EGL loading | `--enable-dynamic_egl` | The A30 minimal probe hit SIGSEGV after the `eglGetDisplay` fallback; do not use it. |

## Current Result

RetroArch 1.22.2 minimal:

- `video_driver = "gl"`
- `video_context_driver = "mali_fbdev"`
- `PLUMOS_RA_DISPLAY_ROTATION=ccw`
- A30 GL2 menu MVP patch applied
- Horizontal RGUI was visually confirmed on the A30

libretro core smoke:

```sh
./scripts/docker-build.sh libretro-cores
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-libretro-cores.sh --duration 6
```

- `fceumm` commit: `3a84a6fd0ba20dd4877c06b1d58741172148395f`
- `gambatte` commit: `6716e6ee39c2abd3ea325f66fb26e7f866f4c5dc`
- `core-info` commit: `f8b241d098e74c668001bb0aba8129b40afe7d03`
- `fceumm` + NES: ROM load, save-path creation, and Mali fbdev display loop confirmed
- `gambatte` + GB: ROM load and Mali fbdev display loop confirmed
- Both game screens were visually confirmed by the user
- Sound is untested because `retroarch-minimal.cfg` disables audio

## Next Practical Build Target

The first full runtime should keep the minimal video path and add only the next
needed pieces:

- Enable OSS audio and run a short sound test with a core.
- Enable/compare ALSA only if `libasound` and device behavior are stable.
- Start `plumos-joystickd --device-mode xbox` and check whether RetroArch input
  works through SDL2/udev or linuxraw.
- Enable `--enable-command` and test safe `QUIT`/save-state actions from the FE.
- Enable/compare CHD/FLAC before PS1/PCE CD/Mega CD/Neo Geo CD core smoke tests.
