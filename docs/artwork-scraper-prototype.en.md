# Artwork Scraper Prototype

This note defines the shell-based thumbnail scraping prototype to validate the
matching and storage rules before any FE integration. The prototype script is
`scripts/prototype-thumbnail-scraper.sh`.

## Goals

- When a ROM CRC matches the libretro database, fetch thumbnails by canonical
  game name.
- Expect many CRC misses and try a filename fallback.
- Keep scraped thumbnails and user-provided thumbnails in one location.
- Do not add FE background downloads, progress UI, or retry UI yet.

## Canonical Storage

The scraper output, user-placed thumbnails, and frontend lookup all use:

```text
/mnt/SDCARD/Images/<system_id>/<relative stem from the ROM alias root>.png
```

Examples:

```text
Roms/FC/Nintendo/Mario.nes
=> Images/nes/Nintendo/Mario.png

Roms/GB/Dracula Densetsu.gb
=> Images/gb/Dracula Densetsu.png
```

## Test Inputs

Current host-side test ROM roots:

```text
artifacts/reference/nes
artifacts/reference/gb
```

`artifacts/reference/nes` also contains `.fds` and `.zip` files. `.fds` files
are handled as `system_id=fds` and are saved under `Images/fds`. `.zip` files
use the first `.nes`, `.fds`, or `.gb` member for CRC calculation.

## Sources

| `system_id` | libretro playlist | DAT source |
| --- | --- | --- |
| `nes` | `Nintendo - Nintendo Entertainment System` | `metadat/no-intro/Nintendo - Nintendo Entertainment System.dat` |
| `fds` | `Nintendo - Family Computer Disk System` | `metadat/no-intro/Nintendo - Family Computer Disk System.dat` |
| `gb` | `Nintendo - Game Boy` | `metadat/no-intro/Nintendo - Game Boy.dat` |

The default thumbnail kind is `Named_Boxarts`. The prototype also accepts
`--kind Named_Snaps` and `--kind Named_Titles`.

## Matching Order

1. If the destination PNG already exists, return `exists`.
2. Compute CRC32 for the ROM payload.
3. Look up `crc -> canonical game name` in the per-system DAT index.
4. If the canonical thumbnail exists on the thumbnail server, return `crc-exact`.
5. If the ROM file stem exists on the thumbnail server, return `name-exact`.
6. Only when `--loose-index` is passed, fetch the large thumbnail directory
   index and try normalized matching as `crc-loose` or `name-loose`.
7. If all matching fails, return `no_match`. Negative cache is updated only in
   `--fetch` mode.

Dry-run does not update the negative cache.

When `--candidate-report <path>` is passed, each `no_match` ROM gets ranked
suggestions from the thumbnail directory index. Suggestions are advisory and
are never auto-downloaded. If the CRC lookup produced a canonical name, that
name is preferred; otherwise the ROM file stem is used for candidate matching.
By default, up to 5 suggestions with score `60` or higher are emitted per ROM.

Candidate TSV columns:

```text
status system source score crc rom_path dest_path query_name thumbnail_name url
```

## Examples

Resolve only:

```sh
scripts/prototype-thumbnail-scraper.sh --dry-run --limit 20
```

Resolve GB only:

```sh
scripts/prototype-thumbnail-scraper.sh --dry-run --system gb --limit 20
```

Fetch into a safe temporary directory:

```sh
scripts/prototype-thumbnail-scraper.sh \
  --fetch \
  --limit 5 \
  --image-root /tmp/plumos-thumb-images
```

Write candidate suggestions for unmatched ROMs:

```sh
scripts/prototype-thumbnail-scraper.sh \
  --dry-run \
  --retry-negative \
  --candidate-report artifacts/thumb-scraper/reports/candidates.tsv
```

Fetch into the final A30 root:

```sh
PLUMOS_SDCARD_ROOT=/mnt/SDCARD \
  scripts/prototype-thumbnail-scraper.sh --fetch --system nes
```

## Before FE Integration

- Split `no_match` results into CRC misses and thumbnail misses.
- Decide where `--loose-index` is worthwhile.
- If serial HEAD/download is too slow, determine shell prototype concurrency
  before moving the design into an FE queue.
- Keep originals under `Images/<system_id>` and put disposable resized render
  cache under something like `/mnt/SDCARD/plumos/cache/frontend/render-cache`.
- On-device testing must confirm network service availability, free space, and
  `.tmp` cleanup after interruption.
