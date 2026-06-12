# libretro core version inventory

Investigation date: 2026-06-13

This note records the version/provenance inventory for Onion-side libretro cores
and their plumOS recipe coverage. The detailed table is
`docs/libretro-core-version-inventory.tsv`.

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
```

Include Onion builder-only recipes with:

```sh
./scripts/inventory-libretro-core-versions.py --include-builder-only
```
