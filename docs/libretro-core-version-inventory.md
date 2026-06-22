# libretro core version inventory

Investigation date: 2026-06-18

This note records the version/provenance inventory for Onion-side libretro cores
and their plumOS recipe coverage. The detailed table is
`docs/libretro-core-version-inventory.tsv`. The upstream source commits resolved
from Onion's proven build window are recorded in
`docs/onion-libretro-source-lock.tsv`.

## Inputs

- Onion repository: `artifacts/onion/Onion`
  - commit: `07505ea58c7bba698d6b9220ff43946a43cac76b`
  - prebuilt cores: `static/build/RetroArch/.retroarch/cores/*_libretro.so`
- Onion builder repository: `artifacts/onion/Miyoo-Mini-Retroarch-builder`
  - commit: `9efb4acbe95a9e7a370487d7fc1b9b7d40fda9c0`
  - recipes: `cores-onionos`, `cores-onionos-special`
- plumOS recipe: `docker/plumos-toolchain/libretro-core-recipes.tsv`

## Policy

- `prefer_onion_version`: the core exists as an Onion prebuilt and also has a
  plumOS recipe. Prefer Onion's `.info` `display_version`, the Onion repository
  commit that last updated the prebuilt binary, and the Onion builder `repo/ref`.
  plumOS still builds from source with the Docker toolchain; it does not ship
  Onion prebuilt binaries as runtime cores.
- `missing_from_plumos`: the core exists as an Onion prebuilt but has no plumOS
  recipe yet. Source provenance, A30 buildability, and practicality need follow-up.
- `plumos_only_latest`: the core exists only in plumOS recipes. Currently this is
  QuickNES, which should build from upstream `HEAD`.

## 2026-06-13 Summary

| policy | count | note |
| --- | ---: | --- |
| `prefer_onion_version` | 96 | Exists in both Onion prebuilt cores and plumOS recipes. |
| `missing_from_plumos` | 14 | Exists in Onion prebuilt cores but not in plumOS recipes. |
| `plumos_only_latest` | 1 | QuickNES. Not an Onion prebuilt core, but retained by plumOS. |

## Onion Prebuilt-Only Core Triage

The 14 `missing_from_plumos` rows are not current normal FE candidates. Most are
either covered by a confirmed plumOS route, are game/application cores for
systems that plumOS does not expose yet, or are KM/custom prebuilt variants whose
source provenance still needs a dedicated recipe decision. This table records the
current reason they were not imported into `libretro-core-recipes.tsv`.

| core | Onion role | current plumOS decision |
| --- | --- | --- |
| `a5200` | Atari 5200-specific core based on Atari800 2.0.2. | Not imported yet because Atari 5200 and Atari 8-bit are already exposed through `atari800`, with system-specific core options in RA and PICO. `a5200` may still be worth evaluating later as a simpler 5200-only route. |
| `arduous` | Arduboy core. Source hint: `https://github.com/libretro/arduous.git`, CMake. | Not imported because Arduboy is not currently a plumOS FE system. Add only if an Arduboy system definition, ROM directory, and legal test content are selected. |
| `dosbox_pure_0.9.7` | Legacy DOSBox-Pure 0.9.7 prebuilt. | Not imported because plumOS already builds and exposes `dosbox_pure` for DOS. Keep only one DOSBox-Pure route unless a regression requires pinning this exact legacy version. |
| `fbalpha2012_cps3` | CPS-3-only FB Alpha 2012 variant for RAM-limited devices. Source hint: `https://github.com/libretro/fbalpha2012_cps3.git`, `svn-current/trunk`, `makefile.libretro`. | Not imported because CPS3 is already covered by confirmed `fbneo` and full `fbalpha2012` routes. Onion's `.info` also describes this variant as a RAM-saving special case and recommends FBNeo for most users. |
| `gong` | Self-contained Pong clone core, no ROM extension. Source hint: `https://github.com/libretro/gong.git`. | Not imported because plumOS FE is ROM-directory driven and this is a built-in game rather than an emulator route. It can be reconsidered as an Apps/game entry, not as a normal ROM system. |
| `km_duckswanstation_xtreme_amped` | KMFDManic Duck/SwanStation PSX variant with hardware-renderer requirements. | Not imported because PlayStation already has confirmed `pcsx_rearmed` RA/PICO/standalone routes. This variant requires modern GL/Vulkan/D3D-style hardware rendering that is not a good A30 target. |
| `km_mame2003_xtreme` | KMFDManic MAME 2003 Xtreme arcade variant. | Not imported because arcade already has confirmed `fbneo`, `fbalpha2012`, `mame2000`, and `mame2003_plus` routes where practical. KM source provenance and ROM-set policy would need a separate arcade decision. |
| `km_puae_xtreme_amped` | KMFDManic P-UAE Xtreme Amped Amiga variant. | Not imported because Amiga is already exposed through confirmed `puae` RA/PICO routes. Treat this as a custom performance variant requiring separate provenance and A30 comparison before adoption. |
| `km_superbroswar` | Super Bros War game-engine core. | Not imported because plumOS has no Super Bros War system/asset policy. It should be considered only if a dedicated game-engine entry is wanted. |
| `mba_mini` | Arcade/FBA/MAME hybrid mini core. | Not imported because it overlaps with the existing arcade/CPS/Neo Geo routes and uses its own ROM-set assumptions. Source provenance and practical ROM-set value need a separate arcade decision. |
| `puae2021` | Older PUAE 2021 branch pinned around `libretro-uae` 2.6.1. Source hint: `https://github.com/libretro/libretro-uae.git`, checkout `2.6.1`. | Not imported because plumOS already builds and exposes `puae`, and Amiga has been confirmed through that route. Keep this as a fallback candidate only if current `puae` regresses. |
| `puzzlescript` | PuzzleScript engine. Source hint: `https://github.com/nwhitehead/pzretro.git`. | Not imported because PuzzleScript is not currently a plumOS FE system. Add only with a system definition, `.pz` ROM directory policy, and legal test content. |
| `sameduck` | Mega Duck / Cougar Boy core. Source hint: `https://github.com/libretro/sameduck.git`, branch `SameDuck-libretro`, subdir `libretro`. | Not imported because Mega Duck is not currently a plumOS FE system and no verification content has been selected. |
| `uae4arm` | Older lightweight Amiga core. Source hint: `https://github.com/libretro/uae4arm-libretro.git`. | Not imported because `puae` is already the confirmed Amiga route. Onion's `.info` positions UAE4ARM as a fallback for very weak devices when P-UAE is unavailable or too slow. |

## Regenerate

```sh
./scripts/inventory-libretro-core-versions.py > docs/libretro-core-version-inventory.tsv
./scripts/resolve-onion-libretro-source-lock.py > docs/onion-libretro-source-lock.tsv
./scripts/apply-onion-source-lock-to-recipes.py --output docker/plumos-toolchain/libretro-core-recipes.tsv
```

Include Onion builder-only recipes with:

```sh
./scripts/inventory-libretro-core-versions.py --include-builder-only
```

Important `onion-libretro-source-lock.tsv` columns:

- `onion_binary_date`: when the prebuilt core last changed in the Onion repo.
- `embedded_build_date`: build/compile date candidates found inside the core binary.
- `lock_date`: the timestamp used to resolve the source commit. Embedded build
  dates are preferred when available; otherwise the Onion binary change date is used.
- `resolved_source_commit`: the newest source commit at or before `lock_date`.
- `resolution_status`: only `resolved` rows are used for recipe pinning.

## Source Lock Notes

Date strings embedded in core binaries can come from game databases or ROM names,
not from the build itself. The resolver therefore does not trust every
`embedded_build_date` unconditionally. If an embedded date is more than 180 days
older than the Onion binary update timestamp, the resolver falls back to
`onion_binary_date`. This was needed for `fbneo`, `mame2000`,
`mame2003_plus`, and `tyrquake`.

Some cores cannot be reproduced from the Onion-era timestamp alone as a
buildable source tree. Those rows are marked with `source_override` in
`onion-libretro-source-lock.tsv` notes:

- `fake08`: uses the `aebd6b9` commit explicitly named by the Onion commit message.
- `fbneo`: uses the nearest commit that still contains the libretro Makefile path
  expected by the Onion recipe.
- `pcsx_rearmed`: the Onion-window `Makefile.libretro` stops as unmaintained, so
  plumOS pins a buildable libretro fork commit.
- `scummvm`: the Onion-window source lacks the current libretro backend, so
  plumOS pins a buildable libretro backend commit.
- `tic80`: the `nesbox/TIC-80` 0.80-era source references an unfetchable submodule,
  so plumOS builds from the libretro wrapper repository. The wrapper CMake default
  uses `BUILD_STATIC=OFF`, which leaves the Lua script backend unregistered, so the
  plumOS build adds `-DBUILD_STATIC=ON`.

As of 2026-06-18, applying this source lock to
`docker/plumos-toolchain/libretro-core-recipes.tsv` and running
`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4`
builds all 97 source recipes with `built=97`, `failed=0`, and `skipped=0`.
