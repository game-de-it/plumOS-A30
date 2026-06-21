# Emulator Issue Triage

This document records how remaining emulator/core problems were investigated
after the full A30 hardware verification pass. Individual verification results
live in `docs/emulator-runtime-verification.tsv`; FE-executable target lists
live in `docs/emulator-fe-libretro-targets.tsv` and
`docs/emulator-fe-standalone-targets.tsv`.

Japanese counterpart: [emulator-issue-triage.ja.md](emulator-issue-triage.ja.md)

## Current Summary

As of 2026-06-19, `emulator-runtime-verification.tsv` contains:

| status | count | meaning |
| --- | ---: | --- |
| `pass` | 168 | video/audio/input/performance are practical |
| `pass_init` | 46 | boot or initial display works; gameplay/input/audio/performance still needs more confirmation |
| `retired` | 52 | removed from normal FE/verification targets by policy |
| `untested` | 6 | not tested because required BIOS/ROM data is unavailable |

## Triage Rules

- If both RA and PICO fail for the same core, start with core recipe, BIOS/system
  path, content layout, and core options.
- If RA works and only PICO fails, suspect PicoArch frontend behavior, RGB565
  presenter, SDL1 input, frame pacing, or PicoArch core-option initialization.
- If performance is clearly insufficient and the same system has a practical
  alternative, prefer removing the profile from default/normal candidates over
  spending time on marginal optimization.
- If a standalone route already works well, lower the priority of equivalent
  libretro routes.
- When a fix is confirmed, update the runtime TSV and FE target TSV. When a
  failure is understood and not worth carrying as a normal route, mark it
  `retired` or non-default.

## Resolved Groups

| group | affected profiles | resolution |
| --- | --- | --- |
| Atari 5200 / Atari 8-bit | `retroarch:atari800`, `picoarch:atari800` | Added system-specific `atari800` core option selection for RA and PICO seed switching with `PLUMOS_PICOARCH_SYSTEM`; moved to `pass_init`. |
| mGBA | GB/GBA `retroarch:mgba`, `picoarch:mgba` | Restored required source files so `projectName` and `anonymousMemoryMap` resolve; runtime package overlays the fixed mGBA core after older all-core artifacts. |
| Commodore 64 | `frodo`, `vice_x64` on RA/PICO | Confirmed normal video with no-ROM/minimal PRG/P00 tests; failing Pacmania sample was a content-format mismatch rather than a renderer failure. |
| X68000 | `retroarch:px68k`, `picoarch:px68k` | Added uppercase BIOS-name lookup for `IPLROM.DAT` / `CGROM.DAT`; both RA/PICO reach the Human68k boot screen. |
| TIC-80 | `retroarch:tic80`, `picoarch:tic80` | Added PicoArch frame-time callback dispatch and built TIC-80 with `-DBUILD_STATIC=ON`. |
| ChaiLove | `retroarch:chailove`, `picoarch:chailove` | Export `SDL_VIDEODRIVER=LIBRETROvideo` for ChaiLove because the core internally uses SDL1. |
| Fairchild Channel F | `retroarch:freechaf`, `picoarch:freechaf` | Added BIOS preflight for required `sl31254.bin` plus `sl31253.bin` or `sl90025.bin`; remains `untested` until BIOS is available. |
| Wolfenstein 3D | `retroarch:ecwolf`, `picoarch:ecwolf` | Seed ECWolf palette as XRGB8888 to avoid RGB565 stripe corruption. |
| ZX-81 | `retroarch:81`, `picoarch:81` | Avoid RetroArch SELECT hotkey conflict and enable fast load by default. |
| PicoArch fceumm | NES/FDS `picoarch:fceumm` | Use a PicoArch-compatible fceumm core built with `platform=miyoomini` for this profile. |
| PicoArch menu input | PicoArch menu/controller settings | Filter duplicate key routes and key-up events, centralize input through joystickd, and drain input queues around bind entry. |
| PicoArch skipped frames | cores returning `video_refresh(NULL, ...)` | Redraw and swap the previous texture on skipped frames to avoid flicker/frame pacing collapse. |
| Arcade PICO performance | Arcade `fbneo`, `fbalpha2012`, `mame2000` | RA remained more stable on A30; Arcade PicoArch companions were removed from normal candidates. |
| PicoArch TyrQuake | `picoarch:tyrquake` | Delay controller setup until after content load to avoid TyrQuake command-path crash. |
| PicoArch Lutro | `picoarch:lutro` | Implement minimal PicoArch perf interface required by Lutro. |
| PicoArch analog axes | PicoArch globally | Default the launcher to corrected `axisYR` / `axisXR` and do not use analog axes for menu navigation. |
| EasyRPG libretro | `retroarch:easyrpg`, `picoarch:easyrpg` | Boot was confirmed, but MP3 warnings remain and standalone is the practical route; libretro route retired. |
| SquirrelJME PICO | `picoarch:squirreljme` | Reclassified as core/content compatibility because RA is the practical confirmed route. |
| Neo Geo MAME2003+ | Neo Geo `mame2003_plus` | Failure was ROM-set mismatch for `fatfury1.zip`; Neo Geo defaults use FBNeo/FBA routes instead. |
| Neo Geo CD FBNeo | RA/PICO `fbneo` | `neocd` is the confirmed gameplay route; FBNeo route retired for Neo Geo CD. |
| PICO-8 | `fake08`, `retro8` | Default moved to `picoarch:fake08`; `retro8` routes retired because BGM remains abnormal. |
| Cave Story / NXEngine | `nxengine` | Japanese data text issue is common to RA/PICO; current route disabled/retired. |
| LowRes NX / VMU / Lutro video | related RA/PICO profiles | Fixed LowRes NX pixel-format timing and kept the route that passes user/device validation. |
| P4 performance limit | heavy cores including `opera`, `virtualjaguar`, `mednafen_pcfx`, `uw8`, `uzem`, heavy GBA, non-fast PCE, RA ScummVM | Verified that RA and PICO were already using hardware display paths; remaining limits are treated as performance retirements. |

## Priority Buckets

### P1: Whole-system blockers

None remain.

### P2: Alternative exists but another route would be useful

Resolved or reflected into default/profile policy.

### P3: PicoArch common-layer issues

Resolved or reclassified into core/content/per-system issues.

### P4: Performance-limit candidates

No active P4 work remains. Candidates were confirmed to be on hardware display
paths and then retired when performance was still not practical.

## Next Investigation Order

P2/P3/P4 and `fail_*` cleanup is complete. Remaining work is waiting on legal
BIOS/ROM availability or user gameplay confirmation for `pass_init` profiles.
