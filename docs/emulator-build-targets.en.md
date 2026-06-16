# plumOS emulator build targets

This document inventories emulator, libretro core, and standalone runtime build
targets for plumOS using Miyoo A30 stockOS `/mnt/SDCARD/Emu`,
`/mnt/SDCARD/RApp`, and `/mnt/SDCARD/RetroArch/.retroarch/cores` as discovery
input, then reclassifies them by expected A30 playability.

This is not a table of stockOS categories or versions to adopt. The plumOS FE
should not use stock MainUI "categories" as its design axis. Instead, it should
model systems/ROM sets and let SELECT choose a core/emulator launch profile.

For RetroArch itself, standalone emulators, and helper libraries, plumOS should
start from the upstream latest stable release available at build time. For
libretro cores, import the cores Onion carries and prefer Onion's proven
commit/build recipe when one exists. plumOS-only cores that Onion does not carry
may still use upstream latest/HEAD as candidates. Only compare against stockOS
versions, patches, or build options when hardware testing shows a regression,
incompatibility, or A30-specific problem.

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
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | Stock core names are reference only. The current recipes are the union of Onion-adopted cores and plumOS-only cores, keeping the plumOS-default 41 Class A/B cores plus Onion-catalog Class O entries in `docker/plumos-toolchain/libretro-core-recipes.tsv`. On real A30 hardware, `fceumm` and `gambatte` have screen-smoke confirmation, and the Onion-proven `mednafen_vb` commit has confirmed performance recovery plus working Bad Apple gameplay; the rest still need per-system boot, performance, input, and audio/video validation. |
| PicoArch | `/mnt/SDCARD/plumos/emulators/picoarch/` | Builds `shauninman/picoarch` commit `802047c` with plumOS `platform=plumos`. SDL1 remains for input/audio, while in-game libretro RGB565 frames are uploaded directly through an A30 Mali/EGL presenter. On 2026-06-15, `fceumm` + `Ikki` confirmed correct orientation, no mirror flip, Native/Aspect/Full sizing, Scanline/DMG/LCD, and stable 60fps audio. It is exposed in Core Settings as test `picoarch:<core>` companions for existing `retroarch:<core>` candidates, but it is not made the initial default. |
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

Starting 2026-06-13, `docker/plumos-toolchain/libretro-core-recipes.tsv` is the
source of truth for libretro core recipes. The default
`PLUMOS_CORE_FILTER=plumos` targets the 41 Class A/B cores; `PLUMOS_CORE_FILTER=onion`
or `all` also includes Onion-catalog Class O entries. Use
`scripts/inventory-onion-libretro-cores.sh` to inspect Onion cores missing from
the plumOS recipe and intentional plumOS-only cores such as QuickNES.
Onion core `display_version`, prebuilt binary update commits, builder recipes,
and plumOS recipe mappings are recorded in
`docs/libretro-core-version-inventory.tsv`; see
[libretro core version inventory](libretro-core-version-inventory.en.md).

The older 2026-06-07 bulk build staged 41 Class A/B cores under
`dist/plumos-libretro-cores/plumos/retroarch/cores`, and its manifest reported
`built=41`, `failed=0`, and `skipped=0`. Keep that as a historical baseline; new
rebuilds should use the union recipe of Onion-adopted and plumOS-only cores.

`vecx` needed `platform=armv HAS_GPU=0` because upstream HEAD's default build
expects OpenGL headers. This keeps it on a software path for the A30
fbdev/Mali/SDL-oriented target. ScummVM uses the libretro Makefile lite build,
and EasyRPG is currently a minimal-ish first libretro build with ICU/XML and
some auxiliary audio features disabled. These are build successes; practical
usefulness still depends on A30 title/profile testing.

The 2026-06-13 full recipe run with
`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 ./scripts/docker-build.sh libretro-cores`
attempted every recipe and initially reported `built=79`, `failed=18`, and
`skipped=0`. Some long-running special cores were still forced to `-j1` during
that pass, so `build-libretro-cores.sh` now tries the configured `JOBS` value
first and retries the same build condition with `BUILD_JOB_FALLBACKS`, default
`1`, only after a failure. Some older libretro Makefiles emit ARM ELF shared
objects as `*_libretro.dll`; staging now verifies those files with `readelf` and
normalizes them to `*_libretro.so`.

Builds that target multiple cores, such as `PLUMOS_CORE_FILTER=all`, also run
in parallel at the core level. The default is
`LIBRETRO_CORE_BUILD_CONCURRENCY=4`, meaning four cores build at once. `JOBS` is
the per-core `make`/`cmake --build` parallelism; when it is unset, the script
derives it by dividing the container CPU count by the core concurrency. Each
core builds into an isolated temporary staging directory, then the parent
process merges manifests and artifacts in recipe order so parallel jobs do not
write the final manifest or output directories concurrently.
Cores that have already proven to require `-j1` are listed in
`LIBRETRO_SERIAL_CORES` and start directly with `jobs=1`. The default serial set
is `nestopia quicknes gambatte gpsp picodrive mednafen_pce_fast mednafen_supergrafx mednafen_ngp mednafen_lynx handy prosystem gw pokemini mednafen_vb dinothawr mrboom tgbdual`.
This does not relax the build condition; it only avoids a known-failing first
attempt while keeping the same optimization flags and recipe platform.

Cores produced through platform fallback cannot be treated as optimized A30
artifacts. `build-libretro-cores.sh` therefore treats the recipe `make_args` as
the only valid build condition and no longer relaxes failed builds to generic
targets such as `platform=unix`. It still retries the same condition with a
lower `JOBS` value because that does not change the optimization contract of the
output.

The follow-up 2026-06-13 pass fixed the remaining 14 cores by root cause.
`tic80` and `dosbox_pure` had moved their upstream default branch to `main`, so
their recipe refs were updated. `dinothawr`, `freeintv`, `mrboom`, and
`tgbdual` now use `classic_armv7_a7`; `ecwolf`, `frodo`, `nekop2`, and `uzem`
use `unix-armv7-hardfloat-neon`; and `x1` uses `crosspi` so each Makefile enters
its intended ARM optimized branch explicitly. `lutro` uses
`platform=armv7-hardfloat-neon` plus a minimal Lua Makefile patch so
`AR=ar rcu` does not conflict with the command-line `AR` override.
`squirreljme` moved its libretro frontend from `ratufacoat` to the `nanocoat`
CMake target, so it is now a special builder. Current mGBA HEAD needs a minimal
libretro CMake build with scripting/docgen/LTO disabled and generated
`version.c` plus duplicate POSIX `memory.c` removed from the source list.
These 14 cores were verified with
`PLUMOS_CORE_FILTER=mgba,tic80,dosbox_pure,dinothawr,ecwolf,freeintv,frodo,lutro,mrboom,nekop2,squirreljme,tgbdual,uzem,x1 FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores`,
which reported `built=14`, `failed=0`, and `skipped=83`. The default
`PLUMOS_CORE_FILTER=plumos` set also reports `built=41`, `failed=0`, and
`skipped=56` after the same changes.
After adding the serial-core initial job override,
`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores`
now reports `built=97`, `failed=0`, and `skipped=0` across all recipes. The
generated-core inventory is in `docs/libretro-built-cores.tsv`; the candidate
system/core/extension/ROM-directory mapping for `systems.json` is in
`docs/libretro-system-core-map.tsv`.

In the inspected Makefiles, some `platform=armv` branches reset the compiler to
host `gcc`, defeating the Docker cross-compiler environment. The script now
defaults `LIBRETRO_FORCE_MAKE_TOOLCHAIN=1` and passes `CC/CXX/AR/RANLIB` as make
command-line variables. Common flags default to
`LIBRETRO_OPTIMIZATION_PROFILE=speed`, using `-O3` and Cortex-A7/NEON hard-float
flags. More aggressive builds can opt into
`LIBRETRO_OPTIMIZATION_PROFILE=aggressive` or `LIBRETRO_ENABLE_LTO=1`. Cores
whose Makefiles append later `-O2` flags over the common flags need targeted
recipe or minimal Makefile fixes. By default
`LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS=1` aligns Makefile release `-O* -DNDEBUG`
flags with the selected optimization profile. With
`LIBRETRO_OPTIMIZATION_PROFILE=speed`, normal builds use `-O3`, while
speed-first `platform=classic_*` branches use `-Ofast`; set
`LIBRETRO_MAKE_OPT_FLAGS` to override that explicitly.

Libretro core optimization is validated one core at a time, not by the success
count of `PLUMOS_CORE_FILTER=all`. Use `PLUMOS_CORE_FILTER=<core>` and, when
testing candidate build options, `LIBRETRO_MAKE_ARGS_OVERRIDE='platform=...'`.
For each successful candidate, inspect the build log for the compiler, final
`-O` flag, Cortex-A7/NEON/hard-float flags, and dynarec/GPU defines, then keep
only the best A30-measured option in `libretro-core-recipes.tsv`. Unverified
generic fallback artifacts are left as failures to investigate.
The Makefile optimization patch checks not only the top-level Makefile but also
shallow `Makefile*`, `*.mk`, and `*.mak` files inside the core tree. This avoids
cases like `gambatte`, where the wrapper `Makefile` includes `Makefile.libretro`
and the included file appends upstream `-O2` after plumOS' `-O3`/`-Ofast`.

PicoArch is treated as an alternative frontend to RetroArch, not as a libretro
core recipe. `./scripts/docker-build.sh picoarch` builds only the PicoArch
binary; it references existing libretro cores under
`/mnt/SDCARD/plumos/retroarch/cores`. The PicoArch binary runs with plumOS glibc
2.36, while SDL1 comes from the A30 stockOS fbcon runtime. If the generic Docker
`libSDL-1.2.so.0` is loaded first, SDL can block in `VT_WAITACTIVE`, so the
PicoArch package stages only glibc/libpng/zlib into an emulator-specific libdir
and the launcher searches `picoarch/lib -> stockOS /usr/lib,/lib,/mnt/SDCARD/miyoo/lib -> plumOS lib`.
On 2026-06-15, the PicoArch-side patched `fceumm` core was also built with
`Makefile.libretro platform=miyoomini`. The generated core used `fceumm` commit
`3f23e2b98f883be9c62a3fdb65c015d376dcd135` and SHA-256
`5fb03352668ac34a86ce341caffe011c728d7f4a03f4d17a4b417d59ef8d591f`; launching it
by full path from `/mnt/SDCARD/plumos/emulators/picoarch/cores/fceumm_libretro.so`
worked. The `/dev/fb0` capture showed the `Ikki` title screen correctly, but the
physical LCD still showed a gray/broken scanout. This confirms the remaining
failure is the stock SDL1 `640x480` fbcon mode set versus A30 LCD scanout, not the
libretro core build. CPU-side rotation was also not performant enough, so the
2026-06-15 fix is to route PicoArch's final frame into a dedicated A30 Mali/EGL
presenter instead of rotating the SDL1 surface on the CPU.

The new `plat_a30_mali.c` path keeps menu rendering on PicoArch's software
640x480 RGB565 surface, but in-game frames bypass PicoArch's CPU scaler and go
directly from the libretro core's RGB565 frame into the A30 Mali/EGL presenter.
It avoids `SDL_SetVideoMode(640x480)` for the physical LCD, dlopens stockOS
`/usr/lib/libEGL.so` and `/usr/lib/libGLESv2.so`, creates a raw panel EGL
surface, uploads RGB565 textures, and presents them with rotated quads. SDL1 is
still used for audio/input initialization. The default
environment is `PLUMOS_PICOARCH_A30_MALI=1`,
`PLUMOS_PICOARCH_A30_ROTATION=ccw`, `PLUMOS_PICOARCH_A30_VSYNC=1`, and
`PLUMOS_PICOARCH_A30_LINEAR=0`. Presenter init failures normally abort; stock
SDL video fallback is allowed only when `PLUMOS_PICOARCH_A30_FALLBACK_SDL=1` is
set explicitly.
The direct path handles PicoArch `Screen size` values `Native`, `Aspect`, and
`Full` with GPU rectangles. `Screen effect` values `None`, `Scanline`, `DMG`,
and `LCD` stay on the GPU path. `DMG` approximates the original white-biased LCD
blend and `LCD` approximates the RGB subpixel pattern in the fragment shader, so
these effects do not return to PicoArch's software scaler on A30.
When an old or user-edited `picoarch.cfg` lacks the Gamepad-side Function menu
binding, startup restores the default menu binding across all SDL input devices.
`EACTION_MENU` is also present in PicoArch's config-save action table, and if an
existing config lacks `bind escape = menu` / `bind \xAA = menu`, startup repairs
and saves it.

A manual `fceumm_libretro.so` launch with `/mnt/SDCARD/Roms/FC/いっき.zip`
logged `picoarch-a30: mali presenter logical=640x480 physical=480x640 rotation=2 vsync=1`,
ran as the sole `/dev/fb0` owner, and the physical screen showed correct
orientation without the mirror flip. Follow-up tests showed that the former
software 640x480 scaling path used about one CPU core and caused audio drops,
while direct present held about 59.9fps with `Native`/`None` and about 60.0fps
with `Scanline`, `DMG`, and `LCD` without continuing underruns. Captures and logs
are under `artifacts/a30-probes/` as `picoarch-native-none-*`,
`picoarch-aspect-*`, `picoarch-scanline-*`, `picoarch-full-*`,
`picoarch-dmg-*`, and `picoarch-lcd-*`. In Core Settings, the FE automatically
adds matching `picoarch:<core>` candidates from existing `retroarch:<core>`
candidates in `systems.json`. This is for multi-system/core validation; as of
2026-06-15, each system keeps its existing `default_launch_profile` as the
initial default.
Follow-up validation fills in PicoArch's missing libretro environment handling,
fixes `RETRO_ENVIRONMENT_SET_CORE_OPTIONS` array handling, and fixes directory
content loading. That removes startup crashes for cores such as `gearboy`,
`gearsystem`, `mednafen_lynx`, and `dosbox_pure`, and fixes directory/no-game
loading paths for `easyrpg`, `scummvm`, and `quasi88`. `pa_log()` is flushed on
each message so probe logs still contain the last initialization stage after
TERM/KILL. `tgbdual` remains excluded from automatic `picoarch:<core>` Core
Settings candidates because a reset state/config still produced a black raw
framebuffer on A30, and `mednafen_pce` is excluded because it runs at roughly
30fps in practice. GB/GBC PicoArch validation should use `gambatte`, `gearboy`,
`mgba`, or `vbam`; PC Engine PicoArch validation should use `mednafen_pce_fast`.
For `quasi88`, D88 header seek/read fails through PicoArch's simple VFS and drops
to the core's `Image not found` menu, so PicoArch returns false only for
`RETRO_ENVIRONMENT_GET_VFS_INTERFACE` and lets the core use its built-in file I/O.
On 2026-06-16, `XeGrader100001.d88` reached the same title screen in both RA and
PICO, with no visible color difference in that comparison.
The launcher also keeps the `picoarch` child PID and kills both `picoarch` and
`plumos-joystickd` during TERM/HUP/INT/EXIT cleanup, preventing stale PicoArch
instances from skewing CPU/audio/fb0-owner tests.

The BIOS/system directory can be set in PicoArch config with `bios_dir = /path`.
When it is absent, PicoArch uses the ROM directory name as a tag and returns
`/mnt/SDCARD/Bios/<tag>`. The 2026-06-15 patch also fixes an off-by-one bug that
could turn `/mnt/SDCARD/Roms/FC/...` into tag `C` instead of `FC`. The plumOS
launcher also reads `/mnt/SDCARD/plumos/config/standalone/picoarch.env`, where
`PLUMOS_PICOARCH_BIOS_DIR=/path` sets the launch default. A per core/ROM
directory `bios_dir` entry in
`/mnt/SDCARD/plumos/state/picoarch/.picoarch-<core>-<tag>/picoarch.cfg` takes
final precedence.

The first 2026-06-13 probe used `scripts/probe-libretro-core-options.sh` on
`nestopia`, `quicknes`, `snes9x2005`, `mednafen_pce_fast`, and
`mednafen_supergrafx`. `nestopia`, `quicknes`, `mednafen_pce_fast`, and
`mednafen_supergrafx` really enter `platform=classic_armv7_a7` with
`-Ofast`/LTO and build through the same-condition `BUILD_JOB_FALLBACKS=1` /
`-j1` retry. `snes9x2005` does not reach its true classic branch because of
Makefile branch ordering, so it uses
`platform=armv7-hardfloat-neon HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon` instead.
The probe logs are under
`artifacts/libretro-core-option-probes/20260613-071812-nestopia`,
`20260613-072334-quicknes`, `20260613-072436-snes9x2005`,
`20260613-072521-mednafen_pce_fast`, and
`20260613-072713-mednafen_supergrafx`.

The second same-day probe compared `gambatte`, `gpsp`, `genesis_plus_gx`, and
`picodrive`. `gambatte`, `gpsp`, and `picodrive` enter
`platform=classic_armv7_a7` with `-Ofast`/LTO and build through the `-j1` retry.
`genesis_plus_gx` does not enable LTO in that branch, but `classic_armv7_a7`
does enable `-Ofast`, with no host compiler use or residual `-O2`. `picodrive`
also had a standalone `-O2` in `platform/common/common.mak` for the Cyclone tool
build, so `*.mak` files are included in the optimization patch. Probe logs are
under `artifacts/libretro-core-option-probes/20260613-075643-gambatte`,
`20260613-074053-gpsp`, `20260613-074216-genesis_plus_gx`, and
`20260613-080709-picodrive`.

The third probe compared `mednafen_ngp`, `mednafen_wswan`, `mednafen_lynx`, and
`handy`. All four use `platform=classic_armv7_a7`. `mednafen_ngp` failed under
the strict recipe `platform=armv` path and only built with classic
`-Ofast`/LTO plus the `-j1` retry. `mednafen_lynx` and `handy` also enter
`-Ofast`/LTO with classic, while `mednafen_wswan` does not enable LTO but does
use `-Ofast`. Probe logs are under
`artifacts/libretro-core-option-probes/20260613-081612-mednafen_ngp`,
`20260613-081750-mednafen_wswan`, `20260613-081818-mednafen_lynx`, and
`20260613-081907-handy`.

The fourth probe compared `stella2014`, `prosystem`, `potator`, `o2em`, `gw`,
and `pokemini`. All six use `platform=classic_armv7_a7`. `prosystem`,
`potator`, `gw`, and `pokemini` enter `-Ofast`/LTO, while `stella2014` and
`o2em` do not enable LTO but do use `-Ofast`. There is no host compiler use or
residual `-O2`. Probe logs are under
`artifacts/libretro-core-option-probes/20260613-082357-stella2014`,
`20260613-082522-prosystem`, `20260613-082606-potator`,
`20260613-082629-o2em`, `20260613-082704-gw`, and
`20260613-082811-pokemini`.

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
| Red Viper | Experimental Virtual Boy probe | The ARM dynarec works on the A30. The StockOS-derived SDL2 `mali` + GLES2 path can be built as `red-viper-sdlgl-a30`, and orientation/fit plus single-eye rendering are fixed. Bad Apple still drops audio in heavy scenes, and a deeper SDL audio queue did not materially improve it, so Red Viper is removed from normal distribution and the FE profile list. Build it explicitly with `PLUMOS_STANDALONE_FILTER=red_viper` only when probing. |

## Virtual Boy Note

Virtual Boy via `mednafen_vb`/Beetle VB was very slow with the old plumOS core
available on 2026-06-12. In those hardware measurements, it reached about
23fps at 1344 MHz / 4 cores with FBO video, and only about 25fps with null
video/audio. The `vb_cpu_emulation=fast` core option is already enabled, and
changing 3D display modes or disabling audio did not get it close to 50fps.

However, on 2026-06-13, the `mednafen_vb_libretro.so` binary from a Miyoo Mini
Plus running Onion 4.3.1 was copied to the A30 as a temporary probe and run
under the same null video/audio benchmark. Bad Apple completed 600 frames in
6.24 seconds, about 96fps. For comparison, the plumOS-built Beetle VB core took
19.60 seconds, about 30.6fps, and the same Onion core on the Miyoo Mini Plus
hardware took 9.14 seconds, about 65.6fps. This means the Beetle VB problem is
probably not simple A30 SoC insufficiency; it is more likely caused by the
plumOS Beetle VB core build, toolchain, or upstream commit. Onion's Virtual Boy
package uses `lr-mednafen-vb`, and its default core option is also
`vb_cpu_emulation=fast`. Onion's RetroArch is a Miyoo Mini/Mini+ Dingux build
with `cpuclock.txt` support for per-core or global CPU clock overrides. Onion's
published guidance recommends 1800 MHz for Mini Plus overclocking, with 1900 MHz
as the stated maximum guideline, but the A30 probe above ran the Onion core at
1344 MHz and was already fast enough. Logs are saved in
`artifacts/a30-logs/mednafen-vb-core-compare-20260613.log` and
`artifacts/miyoo-mini-plus/ra-vb-null-600-scale1.*`.

The Onion repository itself contains this prebuilt core at
`static/build/RetroArch/.retroarch/cores/mednafen_vb_libretro.so`, and its
md5 `cc34723254f0dfc17bce1f2f51a7bbfe` / BuildID
`e510367247958d87064b9def3649b30d50ac80c5` matches the core copied from the
Miyoo Mini Plus. Package Manager
copies selected package directories from `/mnt/SDCARD/App/PackageManager/data`
to `/mnt/SDCARD`; the Virtual Boy package adds the `Emu/VB` launcher/config and
the `Roms/VB` directory, while the launcher references the prebuilt core from
the common RetroArch build area.
The Onion repository CI assembles releases/packages, but this core is treated
as a committed prebuilt binary in the Onion repository; the main Onion CI does
not rebuild Beetle VB from source on every release.

The Onion core provenance points to PR #624 / commit
`bffa64e2c78505444b8dfc4ff0d0439b866b0d79`. Its commit message says the
`beetle-vb-libretro` core was updated from `v1.27.1 (aa77198)` to
`v1.31.0 (162918f)`. The binary's version string is also `v1.31.0 162918f`,
and its compiler string is `GCC: (Debian 8.3.0-2) 8.3.0`. In the official
`libretro/beetle-vb-libretro` repository,
`162918f06d9a705330b2ba128e0d3b65fd1a1bcc` is a 2022-08-28 commit. The current
plumOS build uses upstream `1275bd7` and GCC 12.2.0, so both the source commit
and the toolchain differ from the Onion core.

The likely external builder is `schmurtzm/Miyoo-Mini-Retroarch-builder`. At
builder commit `8a552f2`, just before the 2022-11-21 Onion PR, GitHub Actions
ran the `techdevangelist/miyoomini-buildroot:latest` Docker image, then invoked
`./libretro-buildbot-recipe.sh ../cores-onionos` and `../cores-onionos-special`
inside `libretro-super`. The `cores-onionos` `mednafen_vb` line builds
`https://github.com/libretro/beetle-vb-libretro.git` `master` with
`GENERIC Makefile .`. `cores-onionos.conf` sets
`CC=arm-linux-gnueabihf-gcc`, `CXX=arm-linux-gnueabihf-g++`,
`STRIP=arm-linux-gnueabihf-strip`, `ARM_NEON=1`, `CORTEX_A7=1`, and
`ARM_HARDFLOAT=1`; artifacts are uploaded from `libretro-super/dist/unix/*`.

One caveat: in the relevant `libretro-super` submodule `b941477`, the recipe's
`platform classic_armv7_a7` is not recognized as a dedicated target by
`libretro-config.sh`, so it falls back to `FORMAT_COMPILER_TARGET=unix`.
Therefore, the build recipe that remains visible suggests an actual core build
closer to `make -f Makefile platform=unix -j...` with cross-compiler environment
variables, not `platform=miyoomini` and not a direct invocation of Beetle VB's
`classic_armv7_a7` Makefile section. The Actions artifact/log retention has
expired, so exact hash reproduction still requires rebuilding with the builder
image or a matching GCC 8.3 environment.

Additional 2026-06-13 measurements confirmed that the plumOS toolchain / GCC
12.2.0 can also produce an Onion-speed Beetle VB core when upstream is pinned
to `162918f06d9a705330b2ba128e0d3b65fd1a1bcc`. On the same A30, at
1344 MHz / 4 cores, with RetroArch null video/audio and the Bad Apple 600-frame
benchmark, the current plumOS `1275bd7` core took 18.84 seconds, about
31.85fps; the Onion core took 5.73 seconds, about 104.71fps; and the plumOS
`162918f` + `platform=armv` build took 5.58 seconds, about 107.53fps.
`162918f` + `platform=classic_armv7_a7` reached 5.38 seconds, about 111.52fps,
but GCC 12 LTO requires `-j1`, so it carries build-stability trade-offs. Current
`1275bd7` still took 18.46 seconds, about 32.50fps, even with
`classic_armv7_a7`, so the slowdown is caused primarily by the upstream commit
change, not by build options. The normal plumOS recipe now pins Beetle VB to
`162918f` and uses `platform=classic_armv7_a7` for the speed-first build. With
GCC 12 LTO this condition can fail during the link step at `JOBS=4`, so the
same build condition is retried with `BUILD_JOB_FALLBACKS=1` / `-j1`.

Red Viper is a standalone emulator derived from the 3DS project line, not a
libretro core, but its ARM dynarec runs on the A30 armv7 hard-float target. The
experimental static probe used a raw `.vb` ROM extracted temporarily from zip
and produced these results:

| condition | result |
| --- | --- |
| headless, no audio, no display, 1344 MHz / 4 cores, 1200 frames + 120 warmup | 322.86fps |
| software VIP render, no audio, 1344 MHz / 4 cores, 600 frames + 60 warmup | 289.00fps |
| software VIP render, no audio, 648 MHz / 2 cores, 600 frames + 60 warmup | 135.13fps |

The current Virtual Boy performance problem is resolved by the `mednafen_vb`
libretro core pinned to the Onion-proven commit, so Red Viper is removed from
normal distribution and the FE profile list. The `red-viper-a30`
wrapper copies Red Viper's software framebuffer to `/dev/fb0` in landscape
orientation, handles A30 physical input, opens an in-game menu with Function,
and exits with START+SELECT. The frontend now uses `retroarch:mednafen_vb` as
the only Virtual Boy profile. The A30
wrapper does not include the Red Viper 3DS menu UI; settings are handled through
a small A30 menu and `plumos/config/standalone/red_viper.env`. For the
A30's single display, `PLUMOS_A30_RED_VIPER_EYE=both` is the default and blends
the two eye buffers by taking the strongest red value. The settings inventory
lives in [Red Viper A30 Settings](red-viper-a30-settings.en.md). Audio is
enabled through upstream `vb_sound.c` and ALSA `default` output. The A30 PCM
opens at 48 kHz, so
the Red Viper A30 build defaults to `RED_VIPER_A30_AUDIO_RATE=48000`, ALSA
latency of 160 ms, startup prebuffering, and a producer queue for 10 ms audio
chunks. If the producer queue runs dry, the A30 backend repeats the previous
audio chunk so a temporary ROM-side audio generation delay does not empty the
ALSA buffer. Leaving the upstream 50 kHz audio rate caused audio dropouts and
visible stutter. `plug:default` is not used because it asserts inside the A30
libasound. In the 2026-06-13 hardware check, the 48 kHz build appeared normal
in-game for both audio and video. 7z extraction remains follow-up work. Before
distribution, check the Red Viper/Reality Boy license terms and third-party
notice requirements.

On 2026-06-13, an experimental `red-viper-sdlgl-a30` binary was built from Red
Viper upstream's `source/linux-test` GLES2 frontend and run directly through
stock SDL2's `mali` video driver plus the rootfs `libGLESv2`/`libMali` stack.
With `badapple_mednafen.vb`, it reported `SDL_VIDEODRIVER=mali`,
`gl_renderer=Mali-400 MP`. Short 15 second runs can look close to 50fps in
light scenes, but the 120 second run averaged 35.18fps, bottomed at 14.49fps,
and only spent 9.4% of samples at or above 49fps. It can improve some cases
relative to the fbdev software presentation path, but it does not handle Bad
Apple's peak load. Orientation/fit for this path is now fixed in the GLES final
quad. A 2026-06-13 `ccw` capture confirmed that the raw `480x640` framebuffer
matches the existing FE orientation and becomes readable landscape after the cw
capture transform. The SDL/GLES frontend was also wired to
`PLUMOS_A30_RED_VIPER_EYE`, `PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK`, and SDL
audio queue/prebuffer settings; with `EYE=left`, `present_fps` improved to about
49fps on average. Heavy scenes still fell to the low 40s and audio dropout did
not materially improve with a deeper audio queue, so Red Viper is not built in
normal distribution and is not exposed as an FE profile.

## Class A: initial build targets

These are expected to be satisfying on the A30 and should be included in the
initial plumOS emulator/core build plan.

| system / ROM family | launch profile candidates | stock discovery | note |
| --- | --- | --- | --- |
| FC / NES / FDS | `fceumm`, `nestopia`, optional `quicknes` | `Emu/FC`, `RApp/fceumm`, `RApp/nestopia`, plumOS `quicknes` | Prefer Onion-adopted cores, but keep QuickNES as a plumOS-only lightweight candidate. |
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
| Virtual Boy | `retroarch:mednafen_vb` | The plumOS-built `mednafen_vb` core pinned to Onion-proven commit `162918f` has confirmed Bad Apple gameplay on A30. Red Viper's StockOS-derived rendering path now has correct orientation/fit and single-eye rendering, but heavy-scene audio dropout remains, so it is removed from the FE profile list. |
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
