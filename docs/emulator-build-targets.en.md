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
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | Stock core names are reference only. `fceumm` and `gambatte` are built/deployed from upstream HEAD. Continue preferring upstream latest stable/HEAD. |
| standalone emulators | `/mnt/SDCARD/plumos/emulators/<id>/` | Use for PPSSPP and engines where standalone is better than libretro. |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | Equivalent to stock `Emu/ffplay`; keep outside the initial emulator pack. |

Note: the plumOS-bundled SDL3+sdl2-compat runtime does not provide an A30 real
screen video backend. The RetroArch minimal display probe does not use SDL; it
uses RetroArch's `mali_fbdev` context with the A30 rootfs
`/usr/lib/libEGL.so`/`libGLESv2.so`. RGUI display is confirmed, but real
core-loaded game video is confirmed with `fceumm`/`gambatte`. The current
`retroarch-minimal.cfg` disables audio, so sound and emulator-facing input still
need the next validation step.

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
| PS1 | `pcsx_rearmed` | `Emu/PS`, `RApp/pcsx_rearmed` | Realistic on A30. Decide BIOS/save/state paths early. |
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
| ScummVM | `scummvm` | installed core | Likely low load. Needs mouse/keyboard profiles. |
| EasyRPG | `easyrpg`, optional standalone EasyRPG Player | `RApp/easyrpg` | Practical candidate. |
| DOS classics | `dosbox_pure` | `RApp/dos` | Limit to lightweight DOS games. Needs keyboard profiles. |
| MSX | `bluemsx` | backup `bluemsx` | Not stock top-level, but practical. |

## Class B: promote after targeted checks

These may work, but the satisfaction threshold depends on title, profile, or UX.

| system / ROM family | launch profile candidates | reason |
| --- | --- | --- |
| CPS3 | `fbalpha2012`, `fbneo` | 2D but heavier. Decide from representative titles. |
| SNES enhancement-chip titles | `snes9x`, `snes9x2005-plus`, `mednafen_supafaust` | SA-1/SuperFX/etc. need title-level performance checks. |
| PC-88 / PC-98 | `quasi88`, `np2kai` | Input/keyboard UX may be harder than CPU load. |
| Virtual Boy | `mednafen_vb` | Likely runnable, but screen/UX is special. |
| lightweight PSP | PPSSPP standalone | Test 2D/light titles only; do not promise PSP as a whole. |
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

## Suggested build order

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

## Open checks before building everything

- RetroArch audio/input smoke: minimal RGUI and `fceumm`/`gambatte`
  core-loaded video are confirmed through `fbdev_mali`, but audio and
  emulator-facing input still need device validation.
- Core build recipes need tag/URL/SHA-256 or commit/build options recorded in a
  manifest.
- Save/state/system/BIOS directories must move away from stock
  `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch
  profiles, not a stock `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied
  from stock scripts.
