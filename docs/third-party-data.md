# Third-Party Data

This document records the external data sources that plumOS references or
processes for scraping, artwork lookup, and emulator metadata.

Japanese counterpart: [third-party-data.ja.md](third-party-data.ja.md)

## libretro Database / Thumbnails

plumOS thumbnail scraping uses `libretro-database` and `libretro-thumbnails` as
the standard matching sources.

- Source: https://github.com/libretro/libretro-database
- Source: https://github.com/libretro-thumbnails
- License: the `libretro-database` repository explicitly declares
  `CC-BY-SA-4.0`.
- Use in plumOS: build/prefetch steps convert DAT files and thumbnail directory
  indexes into TSV data. Runtime reads the reduced indexes under
  `/mnt/SDCARD/plumos/share/frontend/artwork-scraper/`.

## Rescue Overlay Seed Data

Some CRCs missing from the libretro base database can still be identified via
third-party sources such as No-Intro / DAT-o-MATIC. plumOS does not redistribute
raw DAT sets wholesale; it stores only the minimal rescue overlay needed to map
verified CRCs to libretro thumbnail labels.

- Source: https://datomatic.no-intro.org/
- Source: https://no-intro.org/
- Source: https://github.com/rinrin-/NesterJ_AoEX_R3/blob/master/src/nesterj/famicom.dat
- Source: https://github.com/finalburnneo/FBNeo
- Use in plumOS: build/prefetch generates kind-specific
  `crc -> libretro thumbnail href` TSV files from verified seed rows.
- Runtime path:
  `/mnt/SDCARD/plumos/share/frontend/artwork-scraper/rescue/<system>/<kind>.tsv`
- Seed path:
  `package/frontend/plumos/config/frontend/scraper-rescue-seeds.tsv`
- Optional build input: `PLUMOS_RESCUE_DAT_ROOTS` or
  `scripts/prefetch-thumbnail-scraper-cache.sh --rescue-dat-root <path>`.

Only rows that meet all of these conditions enter a rescue overlay:

- not duplicated in the libretro base CRC index
- resolved to an existing libretro thumbnail label
- resolved by a verified seed or a unique generation-time match, not by vague
  title similarity
- no runtime DAT download from the A30

Access to No-Intro / DAT-o-MATIC and NesterJ_AoEX_R3 derived data is limited to
build, prefetch, or development verification. The A30 runtime reads only the
bundled reduced TSV files.

NES `nesterj-famicom-dat/date-publisher` seeds include only rows whose CRCs from
NesterJ_AoEX_R3 `famicom.dat` can be uniquely resolved to libretro thumbnail
labels by release date and publisher. Date-only matches with multiple
candidates, or unknown publishers, are excluded.

FBNeo `fbneo-v0.2.97.44/*/single-rom-label` seeds use only local FBNeo console
XML DAT entries that contain a single ROM and have extensions scanned by the
plumOS scraper. Rows are excluded if they overlap the libretro base CRC index,
cannot resolve uniquely to a libretro label, collide with risky region/hack or
prototype tags, or describe split ROM chips.

## Credit

plumOS appreciates the work of the No-Intro, DAT-o-MATIC, NesterJ_AoEX_R3,
FBNeo, libretro-database, and libretro-thumbnails communities. Their databases
and thumbnail indexes make CRC-based artwork lookup practical for user-owned ROM
libraries.
