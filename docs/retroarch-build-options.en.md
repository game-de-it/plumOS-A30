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
| screenshots/images | rpng/rjpeg/screenshots | Enable because manually captured screenshots may be used as gallery/scraping fallbacks. |
| content | `--enable-chd`, `--enable-flac` candidate | Needed if PS1/PCE CD/Mega CD/Neo Geo CD remain in the practical target set. |
| info | core info cache optional | Not required because the FE owns launch profiles, but useful for core metadata. |

## Defer Candidates

| group | feature | reason |
| --- | --- | --- |
| Netplay | Netplay | Stock enables it, but A30 Wi-Fi/CPU make it lower priority. Keep separate from Network Command. |
| LibretroDB | database/playlists | The FE manages ROM scan/metadata, so this is not required for the first full runtime. |
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
- Sound was untested in the minimal build because `retroarch-minimal.cfg`
  disabled audio

RetroArch 1.22.2 practical:

- `./scripts/docker-build.sh retroarch-practical` builds a practical runtime
  with OSS/ALSA audio, SDL2 joypad, Network Command, LibretroDB, 7zip, and
  screenshots/images.
- On the A30, `input_driver = "null"` plus `input_joypad_driver = "sdl2"` and
  `plumos-joystickd --device-mode xbox` detected `plumOS A30 Gamepad`.
- Probes using `udev`/`linuxraw` as the primary input driver failed to
  initialize, so the initial practical config prefers SDL2 joypad.
- OSS audio + SDL2 joypad + `audio_latency = 128` + CPU `performance` had
  working screen, sound, and controls in user visual testing.
- CPU `ondemand` caused audio stutter. CPU `userspace fixed 648000 kHz` with
  2 cores worked well for NES/GB and is the initial lightweight-core default
  for a better stability/battery balance.
- With the power cable unplugged, the dummy CPU load measurement averaged about
  1.98 W for 2 cores + performance and about 8.07 W for 4 cores + performance.
  Since 4 cores consumed about 4.1x more power, core count is also a
  system/ROM override target.
- The FE SELECT core menu shows CPU presets and core-count presets and resolves
  system/ROM overrides into `plumos-retroarch-launch --cpu/--freq/--cores`.
- `--enable-command` is enabled in the practical runtime. Because A30 `lo` can
  be down, the launcher tries `ip link set lo up` before starting RetroArch.
  As of 2026-06-08, `GET_STATUS`/`QUIT` work through `127.0.0.1:55355`.
  The direct launcher path also verified `SAVE_STATE` -> `SAVE_FILES` ->
  `CLOSE_CONTENT` -> `QUIT`, with `GET_STATUS CONTENTLESS` after
  `CLOSE_CONTENT`. The launcher TERM cleanup now uses `SAVE_STATE_SLOT 999` ->
  `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` by default when
  `PLUMOS_RA_SAFE_EXIT=1`, then falls back to kill. The slot is configurable via
  `PLUMOS_RA_SAFE_STATE_SLOT` / `--safe-state-slot`.
  `plumos-safe-shutdown --shutdown --no-poweroff` also verifies the
  FE/text-ui-owned launcher path: it TERM's the launcher and leaves
  `resume-session.json` pending with `auto_state_load=true`. The controller UI
  SAFE menu / START Shutdown path also calls `plumos-safe-shutdown --no-poweroff`
  successfully. The `plumos-safe-hotkeyd` `SIGUSR1` trigger also drives the same
  safe-exit path while RetroArch is running, with resume hold, CPU restore,
  frontend restart, and no residual processes. `plumos-text-ui launch --execute`
  auto-starting `plumos-safe-hotkeyd --oneshot` is also verified.
  `scripts/probe-a30-safe-hotkeyd.sh --trigger signal|physical` can rerun the
  path, and the physical Function press is verified as `trigger source=key`.
  The 2026-06-08 artifact
  `artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes`
  verifies `SAVE_STATE_SLOT 999`, `.state999` creation, and a resume plan using
  `--entry-slot 999`. Next is deciding whether an in-game SAFE menu or direct
  safe-exit is the right UX.

## Next Practical Build Target

The first practical runtime should keep the minimal video path and add only the
next needed pieces:

- Use OSS audio and SDL2 joypad as the initial practical config.
- Enable/compare ALSA only if `libasound` and device behavior are stable.
- The safe-exit flow wired through controller UI SAFE/START and
  `plumos-safe-hotkeyd` is verified through the physical Function press and
  power/sleep backend dry-runs. Next is extending it to an in-game SAFE menu if
  needed, or live-fire real poweroff / real suspend checks.
- Enable/compare CHD/FLAC before PS1/PCE CD/Mega CD/Neo Geo CD core smoke tests.
