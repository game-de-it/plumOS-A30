# plumOS emulator build targets

This document inventories emulator, libretro core, and standalone runtime build
targets for plumOS using Miyoo A30 stockOS `/mnt/SDCARD/Emu`,
`/mnt/SDCARD/RApp`, and `/mnt/SDCARD/RetroArch/.retroarch/cores` as discovery
input, then reclassifies them by expected A30 playability.

This is not a table of stockOS categories or versions to adopt. The plumOS FE
should not use stock MainUI "categories" as its design axis. Instead, it should
model systems/ROM sets and let SELECT choose a core/emulator launch profile.

plumOS should start from the upstream latest stable release available at build
time. Only compare against stockOS versions, patches, or build options when
hardware testing shows a regression, incompatibility, or A30-specific problem.

Inventory date: 2026-06-07

## Policy

- Use stock `Emu/*`, `RApp/*`, and installed cores only as discovery sources.
- Classify plumOS support by system plus launch profile, not by stock category.
- Exclude systems that are unlikely to be satisfying on the A30, even if stockOS
  ships an entry for them.
- Do not reject every CD-based system. PS1, PC Engine CD, Mega CD, and Neo Geo
  CD remain realistic candidates.
- Treat PSP as limited/experimental at first, not as full-system support.
- Treat FFPlay as a media player, outside the emulator pack.
- Use stock `launch.sh` files as inventory only. They should not become the
  official plumOS launch profile format.

## Runtime package targets

| package | plumOS target | note |
| --- | --- | --- |
| RetroArch | `/mnt/SDCARD/plumos/retroarch/bin/retroarch` | RetroArch 1.22.2 minimal RGUI build now confirms real display output through GLES/EGL + `fbdev_mali`. Horizontal A30 RGUI uses a GL2 menu MVP patch plus CCW 90-degree rotation. `fceumm`/`gambatte` core-loaded game screens are also confirmed. Full runtime still needs audio/input validation. Prefer SDL2/evdev input plus `plumos-joystickd --device-mode xbox`. |
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | Stock core names are reference only. The 41 Class A/B cores are now built for the A30 armv7 hard-float target, mostly from upstream HEAD. The staged output is `dist/plumos-libretro-cores`. On real A30 hardware, only `fceumm` and `gambatte` have screen-smoke confirmation so far; the rest still need per-system boot, performance, input, and audio/video validation. |
| standalone emulators | `/mnt/SDCARD/plumos/emulators/<id>/` | Trial builds for PPSSPP, ScummVM, EasyRPG Player, DOSBox Staging, and PCSX-ReARMed are staged in `dist/plumos-standalone-emulators`. After A30 hardware testing, PPSSPP/ScummVM/EasyRPG Player/PCSX-ReARMed are promoted to standalone profile candidates, while DOSBox Staging is kept out of normal launch targets. |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | Equivalent to stock `Emu/ffplay`; keep outside the initial emulator pack. |

Note: the plumOS-bundled SDL3+sdl2-compat runtime does not provide an A30 real
screen video backend. The RetroArch minimal display probe does not use SDL; it
uses RetroArch's `mali_fbdev` context with the A30 rootfs
`/usr/lib/libEGL.so`/`libGLESv2.so`. RGUI display is confirmed, but real
core-loaded game video is confirmed with `fceumm`/`gambatte`. The current
`retroarch-minimal.cfg` disables audio, so sound and emulator-facing input still
need the next validation step.

## Build status

As of 2026-06-07, `./scripts/docker-build.sh libretro-cores` stages 41 Class A/B
cores under `dist/plumos-libretro-cores/plumos/retroarch/cores`. The clean
manifest reports `built=41`, `failed=0`, and `skipped=0`.

`vecx` needed `platform=armv HAS_GPU=0` because upstream HEAD's default build
expects OpenGL headers. This keeps it on a software path for the A30
fbdev/Mali/SDL-oriented target. ScummVM uses the libretro Makefile lite build,
and EasyRPG is currently a minimal-ish first libretro build with ICU/XML and
some auxiliary audio features disabled. These are build successes; practical
usefulness still depends on A30 title/profile testing.

Standalone emulators are staged with `./scripts/docker-build.sh standalone-emulators`
under `dist/plumos-standalone-emulators`. As of 2026-06-07, the trial package
contains `PPSSPP v1.20.4`, `ScummVM v2026.2.0`, `EasyRPG Player 0.8.1.1`,
`DOSBox Staging v0.82.2`, and `PCSX-ReARMed r26l`; the manifest reports
`built=5`, `failed=0`, and `skipped=0`. ScummVM uses a classic engine subset,
DOSBox Staging uses system SpeexDSP plus a fixed page size for cross-build
stability, and PCSX-ReARMed standalone uses the generic SDL1 frontend with
ARMv7/NEON rather than upstream's `miyoo` platform. In the 2026-06-08 A30
hardware pass, PPSSPP, ScummVM, EasyRPG Player, and PCSX-ReARMed completed
first-pass checks for screen, audio, input, menu/exit flow, and config/save
paths. DOSBox Staging can display and accept input after A30 patches, but it is
less practical than DOSBox-Pure libretro because real games push one CPU core
near its limit and audio is more fragile.

## Standalone adoption snapshot

| emulator | decision | A30 result |
| --- | --- | --- |
| PPSSPP v1.20.4 | Standalone default candidate for lightweight PSP | Whole-app rotation, scissor fallback, `854x480` logical UI, L2 menu, A30 gamepad, and fixed 1344 MHz / 4-core CPU profile are confirmed. This is for light titles, not full PSP coverage. |
| ScummVM v2026.2.0 | Standalone default candidate for ScummVM | `rotation_mode=270`, VirtualMouse warp fix, and A30 theme `scummmodern-a30-md` give working screen, mouse, audio, and exit flow. |
| EasyRPG Player 0.8.1.1 | Standalone default candidate for EasyRPG | MP3/mpg123, Vorbis/Opus/MOD/LZH/Freetype+Harfbuzz support is enabled and audio/input/exit flow are confirmed. |
| PCSX-ReARMed r26l | Standalone default candidate for PS1 | Native fb32 rotation, 640x480 landscape-virtual menu, Function menu open/return, shadow clear, input, audio, and game screen are confirmed. |
| DOSBox Staging v0.82.2 | Not a normal target; keep as a probe artifact | SDL2/Mali display and input can work, but it is prone to audio breakup under real-game load. DOS should default to `retroarch:dosbox_pure`. |
| Red Viper | Standalone candidate for Virtual Boy | The ARM dynarec works on the A30. In the 2026-06-12 hardware probe, headless reached 322.86fps, and software VIP rendering still reached 289.00fps at 1344 MHz / 4 cores and 135.13fps at 648 MHz / 2 cores. The `red-viper-a30` wrapper provides fbdev video, A30 input, ALSA audio, and exit cleanup. |

## Virtual Boy Note

Virtual Boy via `mednafen_vb`/Beetle VB is CPU-bound on the A30. In the
2026-06-12 hardware measurements, it reached about 23fps at 1344 MHz / 4 cores
with FBO video, and only about 25fps with null video/audio. The
`vb_cpu_emulation=fast` core option is already enabled, and changing 3D display
modes or disabling audio does not get it close to 50fps.

Red Viper is a standalone emulator derived from the 3DS project line, not a
libretro core, but its ARM dynarec runs on the A30 armv7 hard-float target. The
experimental static probe used a raw `.vb` ROM extracted temporarily from zip
and produced these results:

| condition | result |
| --- | --- |
| headless, no audio, no display, 1344 MHz / 4 cores, 1200 frames + 120 warmup | 322.86fps |
| software VIP render, no audio, 1344 MHz / 4 cores, 600 frames + 60 warmup | 289.00fps |
| software VIP render, no audio, 648 MHz / 2 cores, 600 frames + 60 warmup | 135.13fps |

So Red Viper is a credible fix for the Virtual Boy performance problem. The
`red-viper-a30` wrapper copies Red Viper's software framebuffer to `/dev/fb0`
in landscape orientation and handles A30 physical input plus Function or
START+SELECT exit. The frontend now uses `standalone:red_viper` as the Virtual
Boy default profile and keeps `retroarch:mednafen_vb` as fallback. The A30
wrapper does not include the Red Viper 3DS menu UI; settings are handled through
`plumos/config/standalone/red_viper.env` and frontend launch profiles. For the
A30's single display, `PLUMOS_A30_RED_VIPER_EYE=both` is the default and blends
the two eye buffers by taking the strongest red value. Audio is enabled through
upstream `vb_sound.c` and ALSA `default` output. The A30 PCM opens at 48 kHz, so
the Red Viper A30 build defaults to `RED_VIPER_A30_AUDIO_RATE=48000`, ALSA
latency of 160 ms, and startup prebuffering. Leaving the upstream 50 kHz audio
rate caused audio dropouts and visible stutter. `plug:default` is not used
because it asserts inside the A30 libasound. In the 2026-06-13 hardware check,
the 48 kHz build appeared normal in-game for both audio and video. 7z extraction
remains follow-up work. Before distribution, check the Red Viper/Reality Boy
license terms and third-party notice requirements.

## Class A: initial build targets

These are expected to be satisfying on the A30 and should be included in the
initial plumOS emulator/core build plan.

| system / ROM family | launch profile candidates | stock discovery | note |
| --- | --- | --- | --- |
| FC / NES / FDS | `fceumm`, `nestopia`, optional `quicknes` | `Emu/FC`, `RApp/fceumm`, `RApp/nestopia`, backup `quicknes` | Good first smoke test. |
| GB / GBC | `gambatte`, optional `mgba` | `Emu/GB`, `RApp/gambatte`, backup `gambatte_color` | Low load. |
| GBA | `gpsp`, `mgba`, optional `vba_next` | `Emu/GBA`, `RApp/gpsp`, `RApp/mgba`, backup `vba_next` | `gpsp` for performance, `mgba` for accuracy fallback. |
| Master System / Game Gear / SG-1000 | `genesis_plus_gx` | `Emu/MS`, `RApp/genesis_plus_gx_gg`, backup `genesis_plus_gx_ms/sg` | Can share Sega profiles. |
| Mega Drive / Genesis | `genesis_plus_gx`, `picodrive` | `Emu/MD`, `RApp/genesis_plus_gx`, `RApp/picodrive` | Keep 32X/Mega CD as separate profiles. |
| Mega CD / Sega CD | `genesis_plus_gx`, `picodrive` | `RApp/picodrive`, backup `picodrive_cd` | CD-based but realistic on A30. |
| 32X | `picodrive` | `Emu/MD`, `RApp/picodrive` | Title-dependent, but worth keeping as a candidate. |
| SFC / SNES | `snes9x`, `mednafen_supafaust`, optional `snes9x2005-plus` | `Emu/SFC`, `RApp/snes9x`, `RApp/snes9x2018` | Enhancement-chip games need per-title checks. |
| PC Engine / TurboGrafx-16 | `mednafen_pce_fast` | `Emu/PCE`, `RApp/mednafen_pce_fast` | Low load. |
| PC Engine CD | `mednafen_pce_fast` | backup `mednafen_pce_cd` | Strong non-PS1 CD candidate. Needs BIOS/CHD/CUE checks. |
| SuperGrafx | `mednafen_supergrafx` or `mednafen_pce_fast` | backup `mednafen_supergrafx` | Practical candidate. |
| Neo Geo cartridge | `fbneo` | `Emu/NEOGEO`, `RApp/fbneo` | Needs BIOS and gamelist compatibility checks. |
| Neo Geo CD | `neocd` | `RApp/neocd` | CD-based but 2D-focused. Check loading and CDDA. |
| Arcade 2D | `fbneo`, `fbalpha2012`, `mame2003-plus` | `Emu/ARCADE`, `Emu/Shoot`, `RApp/fbneo`, `RApp/mame2003_plus` | Focus on CPS1/CPS2/Neo Geo/older MAME. |
| PS1 | `standalone:pcsx_rearmed`, `retroarch:pcsx_rearmed` | `Emu/PS`, `RApp/pcsx_rearmed` | Realistic on A30. Standalone is hardware-tested and is the initial default candidate. Decide BIOS/save/state paths early. |
| NGP / NGPC | `mednafen_ngp` | `Emu/NGP`, `RApp/mednafen_ngp` | Low load. |
| WonderSwan / WonderSwan Color | `mednafen_wswan` | `Emu/WSC`, `RApp/mednafen_wswan` | Low load. |
| Atari Lynx | `mednafen_lynx` or `handy` | backup `mednafen_lynx`, installed `handy` | Not stock top-level, but practical on A30. |
| Atari 2600 / 7800 | `stella2014`, `prosystem` | installed cores | Low load. |
| Vectrex / Watara Supervision / Odyssey2 | `vecx`, `potator`, `o2em` | installed cores | Low-load niche systems. |
| Game & Watch | `gw` | backup `gw` | Low load. |
| Pokemon Mini | `pokemini` | backup `pokemini` | Low load. |
| Doom / WAD | `prboom` | `RApp/prboom` | Practical candidate. |
| PICO-8 carts | `retro8`, optional standalone `fake08` | `RApp/retro8`, installed `fake08` | Active stock launch uses `retro8`; standalone `fake08` is a comparison target. |
| TIC-80 | `tic80` | backup `tic80` | Not stock top-level, but practical. |
| ScummVM | `standalone:scummvm`, optional `retroarch:scummvm` | installed core | Standalone has A30 rotation/mouse/theme fixes and is the initial default candidate. |
| EasyRPG | `standalone:easyrpg`, optional `retroarch:easyrpg` | `RApp/easyrpg` | Standalone is the practical candidate with auxiliary audio features enabled. |
| DOS classics | `retroarch:dosbox_pure` | `RApp/dos` | Limit to lightweight DOS games. Standalone DOSBox Staging is not a normal target. Needs keyboard profiles. |
| MSX | `bluemsx` | backup `bluemsx` | Not stock top-level, but practical. |

## Class B: promote after targeted checks

These may work, but the satisfaction threshold depends on title, profile, or UX.

| system / ROM family | launch profile candidates | reason |
| --- | --- | --- |
| CPS3 | `fbalpha2012`, `fbneo` | 2D but heavier. Decide from representative titles. |
| SNES enhancement-chip titles | `snes9x`, `snes9x2005-plus`, `mednafen_supafaust` | SA-1/SuperFX/etc. need title-level performance checks. |
| PC-88 / PC-98 | `quasi88`, `np2kai` | Input/keyboard UX may be harder than CPU load. |
| Virtual Boy | `standalone:red_viper`, fallback `retroarch:mednafen_vb` | Beetle VB stays around 25fps on A30. Red Viper dynarec is confirmed above 135fps even with software rendering. The A30 standalone wrapper provides display/input/ALSA audio/exit cleanup. |
| lightweight PSP | `standalone:ppsspp` | Test 2D/light titles only; do not promise PSP as a whole. A30 input/menu/display are first-pass OK. |
| old computer engines | `crocods`, `gme`, other installed cores | Depends on ROM demand and input profiles. |

## Class C: not initial build targets

These are likely below a satisfying A30 experience. Even if stockOS contains an
entry, do not spend initial plumOS build/test time here unless the user requests
an experimental target or a known-light title.

| system / ROM family | stock/core hint | reason |
| --- | --- | --- |
| Dreamcast | `flycast` | Expected to be too heavy for satisfying play on A30. |
| Nintendo 64 | `mupen64plus`, `parallel_n64` | A few light titles may run, but broad support is unlikely to satisfy. |
| Sega Saturn | `yabause`, `yabasanshiro`, `mednafen_saturn` | Too heavy for A30. |
| 3DO | `opera` | Expected to be too heavy. |
| PC-FX | `mednafen_pcfx` | Expected to be too heavy. |
| current MAME / newer 3D arcade | `mame`, `mame2015` | Class A covers older 2D arcade; newer MAME should wait. |
| heavy PSP | PPSSPP standalone/libretro | PSP as a whole is not a target. Light titles only stay in Class B. |

## Suggested validation order

The Class A/B bulk core build is done. Runtime validation should proceed in this
order after deploying the staged package:

1. RetroArch runtime skeleton and first low-risk cores:
   `fceumm` and `gambatte` are build/deploy/screen-smoke confirmed.
2. Audio/input smoke for the practical RetroArch runtime:
   OSS/ALSA selection, SDL2/evdev or linuxraw input, and `plumos-joystickd --device-mode xbox`.
3. Low-risk 2D set:
   `genesis_plus_gx`, `mednafen_pce_fast`, `mednafen_wswan`,
   `mednafen_ngp`, `snes9x`, `fbneo`.
4. Common handheld/console set:
   `gpsp`, `mgba`, `pcsx_rearmed`, `picodrive`, `mame2003-plus`, `fbalpha2012`.
5. CD systems that are still realistic:
   PS1 via `pcsx_rearmed`, PC Engine CD via `mednafen_pce_fast`,
   Mega CD via `genesis_plus_gx`/`picodrive`, Neo Geo CD via `neocd`.
6. Lightweight systems promoted from stock backup/installed cores:
   `bluemsx`, `mednafen_lynx`/`handy`, `stella2014`, `prosystem`, `vecx`,
   `potator`, `gw`, `pokemini`, `tic80`, `scummvm`, `easyrpg`, `prboom`,
   `dosbox_pure`, `retro8`/`fake08`.
7. Conditional checks:
   CPS3, SNES enhancement-chip titles, PC-88/PC-98, lightweight PSP.

## Open checks after bulk build

- RetroArch audio/input smoke: the practical runtime has confirmed OSS audio
  plus SDL2 joypad and `plumos-joystickd --device-mode xbox` with NES/GB. The
  current default is ALSA `default` plus `Soft Volume Master` and SDL2 joypad,
  but the full bulk-built core set is not validated yet.
- Core build recipes are recorded in `dist/plumos-libretro-cores/docs/manifest.txt`;
  raw per-core details from the doubled run are kept as `manifest.raw-double-run.txt`.
- Standalone emulator build recipes are recorded in
  `dist/plumos-standalone-emulators/docs/manifest.txt`.
- PPSSPP/ScummVM/EasyRPG/PCSX-ReARMed standalone packages have A30 first-pass
  validation. ScummVM directory ROMs resolve the target id from
  `.plumos-scummvm-target`, `scummvm-target.txt`, `.scummvm`, or sibling
  `.scummvm`/`.svm` sidecars, falling back to `sky` only when no sidecar exists.
  DOSBox Staging standalone was tested but is not a normal target.
- DOSBox-Pure libretro can store per-ROM `#EXE` suffixes, ALSA/OSS audio
  driver, audio latency, `dosbox_pure_force60fps`, `dosbox_pure_cycles`, and
  CPU policy/frequency/core-count settings in `core-overrides.json`. On A30,
  `DOS/DOSBOX_DIGGER.ZIP` is configured with `#DIGGER.EXE`, OSS latency 256,
  force60fps, cycles max, and fixed 1344 MHz / 4 cores; the FE A-button path
  reaches `execute: ok`.
- Save/state/system/BIOS directories must move away from stock
  `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch
  profiles, not a stock `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied
  from stock scripts.
