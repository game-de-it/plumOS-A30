# Miyoo Mini Flip Porting Notes

This document is a technical risk map for porting plumOS-A30 to the Miyoo Mini
Flip, abbreviated here as MMF. It is not a statement that the A30 runtime can be
copied as-is. The goal is to preserve the A30 lessons that matter, identify the
A30 assumptions that must be removed, and make the first MMF bring-up less
guessy.

Japanese counterpart: [porting-miyoo-mini-flip.ja.md](porting-miyoo-mini-flip.ja.md)

## Public Hardware Assumptions

Treat public MMF specs as a starting hypothesis only. Verify everything on real
hardware before making code or package decisions.

Public sources commonly describe MMF as:

- SigmaStar SSD202D / dual-core Cortex-A7 around 1.2 GHz.
- 128 MB DDR3 RAM.
- 2.8-inch IPS display around `750x560`.
- Linux / OnionOS-capable SD-card software stack.
- 2.4 GHz Wi-Fi, mono speaker, no analog sticks, and analog triggers.

Useful external references:

- [Retro Handhelds first impressions](https://retrohandhelds.gg/miyoo-mini-flip-first-impressions/)
- [Retro Handhelds setup guide](https://retrohandhelds.gg/miyoo-mini-flip-setup-guide/)
- [Retro Catalog specifications](https://retrocatalog.com/retro-handhelds/miyoo-mini-flip)
- [SteamDeckHQ review/spec summary](https://steamdeckhq.com/news/miyoo-mini-flip-review/)

The two highest-risk public assumptions are the display mode and the input
topology. `750x560` is unusual for the A30 codebase because A30 is built around
a physical `480x640` framebuffer with a `640x480` landscape convention. MMF must
get its own device profile instead of inheriting A30 display constants.

## First-Boot Inventory

Before changing launchers or replacing any stock entry point, collect a clean
hardware and userland inventory from the stock MMF SD card.

Run and archive at least:

```sh
uname -a
cat /proc/cpuinfo
cat /proc/meminfo
cat /proc/cmdline
mount
cat /proc/bus/input/devices
ls -l /dev/fb* /dev/mali /dev/dri /dev/disp /dev/input /dev/snd 2>/dev/null
cat /sys/class/graphics/fb0/name 2>/dev/null
cat /sys/class/graphics/fb0/modes 2>/dev/null
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null
cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null
cat /proc/asound/cards 2>/dev/null
cat /proc/asound/devices 2>/dev/null
amixer scontrols 2>/dev/null
ps w
```

While stock FE, stock RetroArch, and stock standalone emulators are running,
also capture:

```sh
for p in $(pidof MainUI retroarch PPSSPPSDL 2>/dev/null); do
  echo "== $p $(tr '\0' ' ' </proc/$p/cmdline) =="
  ls -l /proc/$p/fd 2>/dev/null
  cat /proc/$p/maps 2>/dev/null | grep -Ei 'SDL|EGL|GLES|Mali|asound|dsp'
done
```

Use `readelf -A`, `readelf -d`, and `strings` on the stock FE, stock SDL
libraries, stock emulators, and GPU/audio libraries. This answers three
questions early:

- ABI: armv7 hard-float, soft-float, or something else.
- Video path: stock SDL backend, direct framebuffer, fbdev EGL, DRM/KMS, or a
  vendor path.
- Audio path: ALSA, OSS `/dev/dsp`, tinyalsa-style helpers, or another mixer
  layer.

## Display And Hardware Rendering

The most important A30 lesson is that generic Linux SDL backends were not
enough. A30 exposes `/dev/fb0` and `/dev/mali`, but no DRM node. The working
plumOS path is a clean-room fbdev + Mali EGL route:

- Open `/dev/fb0` and read the real framebuffer geometry.
- Open the vendor EGL/GLES stack from the rootfs.
- Create a GLES2 context through the fbdev EGL platform.
- Render into an offscreen/logical surface when needed.
- Present exactly one final transformed image to the physical scanout.

On A30 this became the SDL3 `a30mali` backend and the matching frontend/PicoArch
presenters. MMF should not hardcode `a30mali`. Use a new device profile and
either:

- generalize the backend into a profile-driven `plumos-fbdev-egl` backend, or
- add a separate `mmfmali` backend if MMF's EGL setup differs enough.

The MMF display bring-up should proceed in this order:

1. Confirm whether `/dev/mali`, `/usr/lib/libEGL.so`, and
   `/usr/lib/libGLESv2.so` exist.
2. Confirm whether EGL can initialize without a window system.
3. Log `EGL_VERSION`, GL vendor, GL renderer, surface size, and swap result.
4. Draw a known color grid and take both a framebuffer dump and a physical
   photo.
5. Determine physical orientation, logical landscape size, and whether scanout
   is mirrored or rotated.
6. Only after that, run the FE or emulator path.

Do not assume the A30 landscape convention. If MMF really reports `750x560`, the
port needs a scaling policy for non-A30 sizes:

- logical screen used by the FE,
- logical screen reported to SDL2/GLES applications,
- integer and aspect-preserving scaling for emulators,
- thumbnail and screenshot orientation,
- power menu and overlay coordinate space.

If MMF has a working stock SDL video backend, treat it as an investigation
input, not the default dependency. The A30 port initially proved that stock SDL
could show pixels, but the final Pyxel solution became stock-independent because
plumOS-owned SDL made rotation, scaling, cleanup, volume hotkeys, and future
Pyxel updates manageable.

## Input And Hotkeys

The A30 input solution is highly device-specific. A30 has physical buttons on a
kernel input event device, while its analog stick arrives through a serial
protocol on `/dev/ttyS0`. `plumos-joystickd` turns those sources into a virtual
Xbox-like pad through `/dev/uinput`.

Do not assume MMF uses the same topology.

MMF likely has no analog sticks, and public sources mention analog triggers.
That means the A30 serial-stick path may be irrelevant, while trigger handling
may become the new hard part. The first MMF input task is a full event map:

- Identify every `/dev/input/event*` device.
- Press every physical button and trigger while logging `EV_KEY` and `EV_ABS`.
- Check for lid, power, volume, and function/menu events.
- Check whether stock helper daemons create virtual devices through
  `/dev/uinput`.
- Check whether stock FE or daemons grab devices exclusively.

MMF should get a device input profile with at least:

```text
button_event = /dev/input/eventX
power_event = /dev/input/eventY
volume_event = /dev/input/eventZ
has_serial_stick = false
has_analog_triggers = true/false
uinput_gamepad = enabled only if emulators need a normalized pad
sdl_gamecontroller_mapping = generated from MMF, not copied from A30
```

For no-stick hardware, do not start the A30 joystick serial reader. If
standalone apps and SDL2/RetroArch already see sane evdev buttons, a virtual pad
may only be needed to normalize trigger names or function-key behavior.

Power/menu behavior also needs a separate MMF check. A30 uses a safe-hotkey
daemon to show the power menu during FE and emulator execution. MMF may have a
clamshell lid switch. If a lid event exists, document whether it should sleep,
open the power menu, or be ignored until sleep/resume is proven stable.

## Sound, Mixer, And Volume Control

The most useful A30 audio lesson is to avoid hidden mixer bypasses. A standalone
PCSX-ReARMed build that used OSS `/dev/dsp` could play sound, but user volume
control did not work correctly. Rebuilding the standalone path to use ALSA fixed
the practical volume issue.

MMF audio inventory should answer:

- Which devices exist under `/dev/snd`.
- Whether `/dev/dsp` also exists.
- Which ALSA card and mixer controls stock FE uses.
- Whether speaker and headphone use different controls.
- Whether mute/unmute or amplifier GPIO is handled by a stock helper.
- Whether suspend/resume loses PCM, mixer, or amplifier state.

Prefer launch defaults that make audio controllable by plumOS:

```text
SDL_AUDIODRIVER=alsa
RetroArch audio_driver = "alsa"
standalone emulators: ALSA where available, OSS only after volume tests
plumos-volume-control: MMF-specific mixer control names
```

Do not copy A30 mixer names blindly. Even if the SoC family is similar, the
board-level codec, amplifier, headphone detect, and mixer labels may differ.

For validation, always test:

- FE sound effects or UI silence policy.
- RetroArch with one light core.
- A standalone SDL2 app.
- A standalone emulator that previously had OSS/ALSA ambiguity, such as
  PCSX-ReARMed.
- Volume up/down hotkeys during gameplay.
- Sleep/resume while audio is active.

## Boot, StockOS Boundary, And Rollback

On A30, plumOS starts from the stock SD-card `miyoo/app/MainUI` path by replacing
it with a wrapper and preserving `MainUI.stock` for fallback. MMF may use a
different stock boot path, helper process set, or library layout.

Before installing a wrapper on MMF:

1. Locate the stock FE entry point from boot scripts.
2. Confirm the stock SD payload required for fallback.
3. Confirm whether Wi-Fi, key handling, audio amplifier, or brightness is
   initialized by the stock FE, shell scripts, or daemons.
4. Keep rollback SD-card-only.
5. Do not write NAND/rootfs during early bring-up.

The release package for MMF must be built from an MMF stock SD payload, not from
A30 stock files. A30's `miyoo/`, `lib/`, launcher scripts, and fallback binary
are not a valid MMF recovery base.

## Performance Constraints

If public specs are correct, MMF has two Cortex-A7 cores and 128 MB RAM. That is
closer to Miyoo Mini class hardware than A30 class hardware. Consequences:

- Emulator lists and default cores should not be copied from A30 without
  retesting.
- Scraper concurrency should be reduced until memory and CPU headroom are
  measured.
- PicoArch vs RetroArch performance may differ from A30 because presenter,
  audio pacing, and core build flags interact with the device display path.
- Pyxel and standalone GLES apps should be attempted only after the EGL/SDL
  backend is stable.

The A30 lesson from Virtual Boy and Hatari still applies: core version and build
date can matter more than "latest". Keep Onion/source-lock research available,
but rebuild from source for the MMF ABI and performance profile.

## Build-System Refactor Points

Before MMF becomes a first-class target, remove A30-only names from paths and
launch decisions where possible.

Good target split:

```text
device profile:
  miyoo-a30
  miyoo-mini-flip

profile owns:
  framebuffer geometry
  EGL/native-window setup
  SDL video driver name
  input event map
  SDL GameController mapping
  volume mixer controls
  CPU governor/core policy
  stock boot entry point
  release stock payload root
```

A30-specific names may remain in historical docs and A30-only launchers, but new
shared code should not require `A30` in the API unless the behavior is truly A30
only.

Candidate build targets:

- `frontend` with profile-selected renderer constants.
- `sdl2-runtime` with either generic fbdev EGL or MMF-specific backend.
- `joystickd` with a no-serial/button-only mode if MMF has no stick.
- `network-services` reused if busybox/dropbear assumptions still hold.
- `retroarch-practical` and `libretro-cores` rebuilt for the confirmed MMF ABI.
- `picoarch` rebuilt only after presenter/input pacing is proven.

## Minimal Bring-Up Milestones

1. Stock SD backup and hardware inventory.
2. SSH or serial access that survives FE crashes.
3. Framebuffer dump and physical screen photo from a simple color-grid probe.
4. EGL/GLES probe, or a clear decision that MMF lacks usable HW rendering.
5. Button, trigger, power, volume, and optional lid event map.
6. ALSA mixer and volume-control proof.
7. plumOS splash or minimal FE first frame with exactly one framebuffer owner.
8. FE navigation using MMF physical buttons.
9. RetroArch NES/GB smoke test with audio and exit.
10. Sleep/power menu test.
11. Standalone emulator smoke test.
12. Release package built from MMF stock SD root and tested on a fresh SD card.

## Red Flags

Stop and document before moving on if any of these appear:

- No usable `/dev/mali`, `/dev/dri`, EGL, or GLES path.
- Stock SDL displays correctly only through a private backend that cannot be
  reproduced cleanly.
- Framebuffer captures look correct but the physical panel is rotated, mirrored,
  or tiled.
- `/dev/uinput` is missing or blocked.
- Stock key/power daemon grabs input exclusively.
- Volume keys change UI state but not actual PCM/speaker volume.
- OSS `/dev/dsp` playback works but ignores plumOS volume control.
- Sleep/resume restores video but loses audio or input.
- The stock FE initializes hardware that plumOS does not yet initialize.

When a red flag is hit, prefer a small probe and a written conclusion over
patching the full FE or emulator stack blindly.

## A30 Documents To Reuse

The following A30 documents are the best references for MMF porting:

- [A30 Stock SDL Video](../a30-stock-sdl-video.md)
- [Pyxel A30 Output](../pyxel-a30-output.md)
- [A30 joystickd](../a30-joystickd.md)
- [A30 Input Policy](../a30-input-policy.md)
- [A30 System Notes](../a30-system-notes.md)
- [MainUI Bootstrap](../mainui-bootstrap.md)
- [Docker Toolchain Details](../docker-toolchain.md)
