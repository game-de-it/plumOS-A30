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
  so plumOS builds from the libretro wrapper repository.

As of 2026-06-18, applying this source lock to
`docker/plumos-toolchain/libretro-core-recipes.tsv` and running
`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4`
builds all 97 source recipes with `built=97`, `failed=0`, and `skipped=0`.
