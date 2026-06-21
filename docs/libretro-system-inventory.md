# libretro Core / System Inventory

This document describes the inventory created by:

```sh
PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores
```

Japanese counterpart: [libretro-system-inventory.ja.md](libretro-system-inventory.ja.md)

The generated inventory is used to decide which built libretro cores can be
reflected into `systems.json`.

- `docs/libretro-built-cores.tsv`: inventory of the 97 built cores, including
  `*_libretro.info`, build manifest data, and candidate system IDs.
- `docs/libretro-system-core-map.tsv`: per-system candidate table with
  `rom_dir`, supported extensions, default core, and candidate cores.

`status` values:

- `existing`: the same system/core mapping already exists in
  `package/frontend/plumos/config/frontend/systems.json`.
- `add_core`: the system exists and the newly built core can be added as a
  launch profile candidate.
- `add_system`: the system itself is a new candidate. BIOS, performance, input,
  and content layout still need hardware validation.
- `contentless`: the core can start without a ROM file. Whether it should appear
  in the normal ROM scanner is a separate UX decision.

For existing systems, the ROM directory uses the current primary directory alias
from `systems.json`. For new systems, Onion ROM folder names are preferred when
known; otherwise plumOS candidate names use uppercase system-style directory
names.
