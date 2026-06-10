# Artwork Scraper Prototype

This note defines the shell-based thumbnail scraping prototype to validate the
matching and storage rules before any FE integration. The prototype script is
`scripts/prototype-thumbnail-scraper.sh`.

## Goals

- When a ROM CRC matches the libretro database, fetch thumbnails by canonical
  game name.
- Treat CRC misses as `no_match` by default. Filename fallback is limited to an
  explicit advisory candidate option and never auto-saves images.
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

## FE Integration Policy

Keep scraper scope separate from frontend thumbnail display scope. Even when a
system is excluded from scraping, user-provided thumbnails placed under
`/mnt/SDCARD/Images/<system_id>/...` are still displayed normally.

The normal scraper targets cartridge-like systems where a simple ROM payload CRC
can be matched against libretro database metadata.

```text
nes
fds
sfc
gb
gbc
gba
megadrive
mastersystem
gamegear
sega32x
pcengine
n64
ngp
ngpc
wonderswan
wonderswancolor
msx
```

CD-based systems are excluded from the initial scraper because even CRC checks
can be slow on the device and matching also needs track / CHD / ISO / cue / m3u
/ pbp representative-file policy.

```text
segacd
pcenginecd
psx
psp
dreamcast
saturn
```

PC disk-image systems are excluded from the initial scraper because they need a
representative-file policy for D88/HDI/DIM/XDF/m3u and a DAT-source policy.

```text
pc88
pc98
x68000
```

Arcade / romset systems are excluded from the initial scraper because matching
depends on internal ROM members and romset versions rather than a single zip
CRC.

```text
arcade
fbneo
mame2003plus
cps1
cps2
cps3
neogeo
```

Systems that are not naturally identified by a single game-file CRC are also
excluded from scraping.

```text
dos
easyrpg
pico8
scummvm
openbor
ports
tic80
```

CRC miss handling:

- Default behavior returns `no_match`.
- Japanese file stems are not searched as filename candidates.
- Filename-derived candidate search is limited to an explicit user-selected
  advisory option.
- Candidate matches are never auto-saved; only the user-selected candidate is
  saved.

## Concurrency

The scraper uses separate CRC and download queues.

Initial values:

```text
CRC workers default: 1
CRC workers bulk:    2
PNG download default: 2
PNG download bulk:    3
PNG download hard cap: 4
```

CRC workers are overridable per system. Systems with many small ROMs can be
raised after real-device validation, while large-ROM systems such as N64 can
stay at `1`. Unvalidated systems use `default=1`, `bulk=2`, and `max=2`.
Validation can probe up to `max=5`, but the normal FE UI should not expose
unbounded values.

Example future `systems.json` field:

```json
{
  "id": "nes",
  "scraper": {
    "enabled": true,
    "crc_workers": { "default": 1, "bulk": 2, "max": 5 },
    "download_workers": { "default": 2, "bulk": 3, "max": 4 }
  }
}
```

Use `scripts/benchmark-a30-crc-workers.sh` for real-device CRC worker tests. It
does not print ROM filenames; it reports only system, worker count, file count,
and timing.

```sh
A30_TARGET=root@192.168.10.165 \
  scripts/benchmark-a30-crc-workers.sh --system nes --workers "1 2 3 4 5" --limit 100
```

Save measurement results under `artifacts/` before turning them into per-system
policy.

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
4. When CRC matches, derive the URL from the canonical game name and try a
   direct download.
5. Save the image when the download succeeds. Return `no_match` when it fails.
6. When CRC does not match, default behavior returns `no_match`.
7. Only when an explicit option is passed, try ROM-file-stem `name-exact` or
   `candidate-report` lookup.
8. Only when `--loose-index` is passed, fetch the large thumbnail directory
   index and try normalized matching as `crc-loose` or `name-loose`.
9. If all matching fails, return `no_match`. Negative cache is updated only in
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
- Measure per-system CRC worker counts on the A30 with
  `scripts/benchmark-a30-crc-workers.sh` before writing them into
  `systems.json` policy.
- Keep originals under `Images/<system_id>` and put disposable resized render
  cache under something like `/mnt/SDCARD/plumos/cache/frontend/render-cache`.
- On-device testing must confirm network service availability, free space, and
  `.tmp` cleanup after interruption.
