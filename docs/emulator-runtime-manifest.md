# Emulator Runtime Manifest

Updated: 2026-06-19

This document explains the adopted emulator, libretro core, and Pyxel runtime
manifest for profiles that can be launched from the plumOS frontend. The
machine-readable source of truth is `docs/emulator-runtime-manifest.tsv`.

Japanese counterpart: [emulator-runtime-manifest.ja.md](emulator-runtime-manifest.ja.md)

## Positioning

`docs/emulator-runtime-manifest.tsv` lists only launch profiles that satisfy all
of these conditions:

- the FE can launch them
- `verification_status` is `pass`, `pass_init`, or `untested`
- `retired` profiles are excluded as normal candidates

`pass_init` and `untested` are not failures. They remain as adopted or pending
candidates because additional legal ROMs, BIOS files, or hardware operation are
needed before the status can advance.

## Current Counts

| item | count |
| --- | ---: |
| runtime profile | 220 |
| `pass` | 168 |
| `pass_init` | 46 |
| `untested` | 6 |
| RetroArch profile | 111 |
| PicoArch profile | 103 |
| standalone profile | 5 |
| Pyxel profile | 1 |
| unique libretro core used by FE candidates | 77 |

`fail_*` statuses are not kept in the adoption manifest. Profiles with
confirmed performance limits, better alternatives, or unresolved input/video/
audio issues are marked `retired` in
`docs/emulator-runtime-verification.tsv` and removed from normal FE candidates.

## Source Files

| file | role |
| --- | --- |
| `docker/plumos-toolchain/libretro-core-recipes.tsv` | Full libretro build recipe with repo/ref/makefile/make arguments. |
| `docs/onion-libretro-source-lock.tsv` | Source commits resolved from the Onion source period. Onion prebuilt binaries are not adopted. |
| `docs/libretro-core-version-inventory.tsv` | Inventory matching Onion prebuilt data, Onion builder data, and plumOS recipes. |
| `docs/libretro-built-cores.tsv` | Inventory of 97 built cores from the latest all-core build manifest. |
| `docs/emulator-fe-libretro-targets.tsv` | FE-executable libretro/RA/PICO profile verification targets. |
| `docs/emulator-fe-standalone-targets.tsv` | FE-executable standalone/Pyxel profile verification targets. |
| `docs/emulator-runtime-manifest.tsv` | Final joined manifest of adopted FE profile, source/ref, and build option data. |

## Regeneration

After rebuilding libretro cores, update the built-core inventory from the build
manifest:

```sh
./scripts/update-libretro-built-core-inventory.py \
  --manifest dist/plumos-libretro-cores-onion-lock-all/docs/manifest.txt \
  --output docs/libretro-built-cores.tsv
```

After updating FE target TSV files, regenerate the adoption manifest:

```sh
./scripts/generate-emulator-runtime-manifest.py \
  --output docs/emulator-runtime-manifest.tsv
```

For libretro cores, `source_ref` prefers the commit recorded in the latest build
manifest. Standalone and Pyxel profiles read the default `*_REF` values from
their build scripts. RetroArch and PicoArch are recorded as `frontend_runtime`
for each libretro profile and are kept separate from the core source/ref.

## Notes

- PicoArch normally uses libretro cores from
  `/mnt/SDCARD/plumos/retroarch/cores`.
- `picoarch:fceumm` prefers the PicoArch package's compatibility core when that
  core exists.
- `quicknes` is a plumOS-only-latest adoption because it is not present in Onion
  prebuilt cores.
- Cores present in Onion prebuilt data but missing from plumOS recipes are not
  added to the FE manifest until source provenance and A30 practicality are
  confirmed.
