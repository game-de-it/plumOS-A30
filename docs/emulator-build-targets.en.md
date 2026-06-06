# plumOS emulator build targets

This document inventories emulator, libretro core, and standalone runtime build
targets for plumOS using Miyoo A30 stockOS `/mnt/SDCARD/Emu`,
`/mnt/SDCARD/RApp`, and `/mnt/SDCARD/RetroArch/.retroarch/cores` as input.

This is not a table of stockOS versions to adopt. plumOS should start from the
upstream latest stable release available at build time. Only compare against
stockOS versions, patches, or build options when hardware testing shows a
regression, incompatibility, or A30-specific problem.

Inventory date: 2026-06-07

## Policy

- Treat `Emu/*` as the stock MainUI primary categories.
- Treat `RApp/*` as extra categories or alternate cores.
- Treat `RApp/backups/*` and cores merely present under RetroArch as later
  optional targets.
- Prefer standalone PPSSPP for PSP; keep libretro PPSSPP as an alternate target.
- Treat FFPlay as a media player, not part of the emulator core pack.
- Use stock `launch.sh` files as inventory only. They should not become the
  official plumOS launch profile format.

## Runtime package targets

| package | plumOS target | note |
| --- | --- | --- |
| RetroArch | `/mnt/SDCARD/plumos/retroarch/bin/retroarch` | Build for A30 armv7 hard-float. Prefer SDL2/evdev input plus `plumos-joystickd --device-mode xbox`. Validate the A30 Mali/fbdev real-display path separately. |
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | Stock core names are reference only. Prefer upstream latest stable. |
| PPSSPP standalone | `/mnt/SDCARD/plumos/emulators/ppsspp/` | Primary PSP target. Needs duplicate-input/serial contention checks with resident `plumos-joystickd`. |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | Equivalent to stock `Emu/ffplay`; keep outside the initial emulator pack. |

Note: the plumOS-bundled SDL3+sdl2-compat runtime does not provide an A30 real
screen video backend. RetroArch display output cannot simply assume generic SDL2
video; fbdev/Mali EGL or another A30-capable path must be validated.

## Tier 1: stock `Emu` categories

Use this table as the first plumOS emulator/core build baseline.

| stock category | ROM dir | stock launch/core | plumOS build target | priority note |
| --- | --- | --- | --- | --- |
| ARCADE | `Roms/ARCADE` | `fbneo`, `fbalpha2012`, `mame2003_xtreme`, `mame` | `fbneo`, `fbalpha2012`, `mame2003-plus`, optional current `mame` | Start with `fbneo`. Current MAME is likely heavy and should wait. |
| Shoot | `Roms/Shoot` | `fbalpha2012`, `fbneo`, `mame2003_xtreme` | `fbneo`, `fbalpha2012`, `mame2003-plus` | Same arcade group as ARCADE. |
| MAME2003PLUS | `Roms/MAME2003Plus` | `mame2003_xtreme` | `mame2003-plus` | Treat the stock `xtreme` fork as a comparison point only when needed. |
| NEOGEO | `Roms/NEOGEO` | `fbneo` | `fbneo` | Also validate BIOS and gamelist compatibility. |
| DC | `Roms/DC` | `flycast` | `flycast` | High-load 3D target. Test after video/input/audio basics are stable. |
| FC | `Roms/FC` | `fceumm`, `nestopia` | `fceumm`, `nestopia` | Good early smoke-test target. |
| GB/GBC | `Roms/GB` | `gambatte`, `mgba` | `gambatte`, `mgba` | `gambatte` first; `mgba` as accuracy/shared fallback. |
| GBA | `Roms/GBA` | `gpsp`, `mgba` | `gpsp`, `mgba` | `gpsp` for performance, `mgba` as accuracy fallback. Rumble profiles can wait. |
| MD/SMS/GG/SG/32X | `Roms/MD`, `Roms/MS` | `genesis_plus_gx`, `picodrive` | `genesis_plus_gx`, `picodrive` | Cover Mega Drive, Master System, Game Gear, SG-1000, 32X, and Mega CD profiles. |
| N64 | `Roms/N64` | `mupen64plus`, `parallel_n64` | `mupen64plus-next`, optional `parallel-n64` | High-load 3D target; be strict on performance. |
| NGP | `Roms/NGP` | `mednafen_ngp` | `mednafen_ngp` | Low-load target. |
| PCE | `Roms/PCE` | `mednafen_pce_fast` | `mednafen_pce_fast` | Check PCE CD / SuperGrafx as extra profiles. |
| PSP | `Roms/PSP` | standalone `PPSSPPSDL`, `ppsspp_xtreme_libretro` | PPSSPP standalone, optional `ppsspp_libretro` | Standalone first. Input daemon contention needs testing. |
| PS1 | `Roms/PS` | `pcsx_rearmed` | `pcsx_rearmed` | Decide BIOS/profile/save paths early. |
| SFC/SNES | `Roms/SFC` | `mednafen_supafaust`, `snes9x` | `mednafen_supafaust`, `snes9x`, optional `snes9x2005-plus` | Stock default is supafaust. Choose default after performance testing. |
| WSC | `Roms/WSC` | `mednafen_wswan` | `mednafen_wswan` | Low-load target. |
| FFPlay | `Video` | `ffplay` | optional FFmpeg/FFPlay | Media player, not emulator pack. |

## Tier 2: active `RApp` categories

These are active top-level stock `RApp` categories. Add them after Tier 1.

| stock RApp | ROM dir | stock core | plumOS build target | note |
| --- | --- | --- | --- | --- |
| `dos` | `Roms/DOS` | `dosbox_pure` | `dosbox_pure` | DOS support; needs keyboard/input profile work. |
| `easyrpg` | `Roms/EASYRPG` | `easyrpg` | `easyrpg` libretro or standalone EasyRPG Player | Start with libretro; compare standalone if needed. |
| `fbahack` | `Roms/FBAHACK` | `fb_32b` | `fbneo`/FBA-family comparison | Stock-specific leaning; dig in only for ROM compatibility needs. |
| `fbalpha2012_cps1/2/3` | `Roms/ARCADE/cps*` | `fbneo` or `fbalpha2012` | `fbneo`, `fbalpha2012` | Arcade profile split. |
| `fbalpha2012_neogeo`, `fbneo` | `Roms/NEOGEO`, `Roms/ARCADE` | `fbneo` | `fbneo` | Same core as Tier 1. |
| `fceumm`, `nestopia` | `Roms/FC` | `fceumm`, `nestopia` | `fceumm`, `nestopia` | FC alternate profiles. |
| `gambatte` | `Roms/GB` | `gambatte` | `gambatte` | GB/GBC profile. |
| `genesis_plus_gx`, `genesis_plus_gx_gg` | `Roms/MD`, `Roms/GameGear` | `genesis_plus_gx` | `genesis_plus_gx` | Sega profile split. |
| `gpsp`, `mgba` | `Roms/GBA` | `gpsp`, `mgba` | `gpsp`, `mgba` | GBA profiles. |
| `mame2003_plus` | `Roms/MAME2003Plus` | `mame2003_xtreme` | `mame2003-plus` | Compare stock fork only if needed. |
| `mednafen_ngp` | `Roms/NGP` | `mednafen_ngp` | `mednafen_ngp` | NGP profile. |
| `mednafen_pce_fast` | `Roms/PCE` | `mednafen_pce_fast` | `mednafen_pce_fast` | PCE profile. |
| `mednafen_supergrafx` | `Roms/SFC` in stock config | `mednafen_supafaust` | `mednafen_supafaust` | Stock config name is misleading; treat as SFC. |
| `mednafen_wswan` | `Roms/WSC` | `mednafen_wswan` | `mednafen_wswan` | WSC profile. |
| `neocd` | `Roms/NEOCD` | `neocd` | `neocd` | Neo Geo CD; needs BIOS validation. |
| `pcsx_rearmed` | `Roms/PS` | `pcsx_rearmed` | `pcsx_rearmed` | PS1 profile. |
| `picodrive` | `Roms/MD` | `picodrive` | `picodrive` | 32X/Mega CD fallback. |
| `prboom` | `Roms/PRBOOM` | `prboom` | `prboom` | Doom/WAD support. |
| `retro8` | `Roms/PICO` | `retro8` | `retro8`, optional standalone `fake08` | Stock dir also has `FAKE08`, but active launch uses `retro8_libretro.so`. |
| `snes9x`, `snes9x2018` | `Roms/SFC` | `snes9x`, `snes9x2005_plus` | `snes9x`, optional `snes9x2005-plus` | Performance fallback. |

## Tier 3: backup/installed optional targets

Stock SD also contains backup configs and installed cores that are not top-level
active categories. Add these when matching ROM directories exist or when the
user needs them.

| family | candidate targets |
| --- | --- |
| MSX | `bluemsx` |
| Atari / classic | `handy` or `mednafen_lynx`, `prosystem`, `stella2014`, `potator`, `vecx`, `freechaf`, `o2em` |
| Nintendo extras | `gw`, `pokemini`, `quicknes`, `vba_next`, `mednafen_vb` |
| NEC computer / console | `np2kai`, `quasi88`, `mednafen_pcfx` |
| 3DO / Saturn | `opera`, `yabause`, `yabasanshiro`, optional `mednafen_saturn` |
| Adventure / engines | `scummvm`, `tic80`, `gme` |
| Other installed cores | `crocods`, `fake08`, `sameduck`, `pocketsnes`, `snes9x2002`, `snes9x2005`, `snes9x2010`, `mame2015`, `mame078plus`, `mame_libretro` |

## Suggested build order

1. RetroArch runtime skeleton and one low-risk core:
   `fceumm`, then video/audio/input smoke test.
2. Low-risk 2D core set:
   `gambatte`, `genesis_plus_gx`, `mednafen_pce_fast`, `mednafen_wswan`,
   `mednafen_ngp`, `snes9x` or `mednafen_supafaust`.
3. Common user-facing systems:
   `pcsx_rearmed`, `gpsp`, `mgba`, `fbneo`, `mame2003-plus`, `fbalpha2012`.
4. Standalone PSP:
   PPSSPP standalone with `plumos-joystickd --device-mode xbox`; check whether
   PPSSPP's extra `/dev/input/event3` and `/dev/ttyS0` opens cause duplicate
   input or serial contention.
5. High-load 3D:
   `flycast`, `mupen64plus-next`, optional `parallel-n64`.
6. Extra `RApp` and backup targets:
   `dosbox_pure`, `easyrpg`, `neocd`, `prboom`, `retro8`/`fake08`, then Tier 3
   as needed.

## Open checks before building everything

- RetroArch real video output path on A30:
  plain upstream SDL2/SDL3+sdl2-compat is not enough for `/dev/fb0` display.
- Core build recipes need tag/URL/SHA-256/build options recorded in a manifest.
- Save/state/system/BIOS directories must move away from stock
  `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch
  profiles, not a stock `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied
  from stock scripts.
