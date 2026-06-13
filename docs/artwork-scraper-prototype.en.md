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
- Use `Apps -> Scraping` for system selection, in-progress status, and latest
  result review.
- Keep retry UI and filename fallback candidate review as separate tasks.

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

## Repeat-Run Policy

On the second and later scraper runs, existing thumbnails are treated as the
user's choice and take priority.

- For each ROM, first look under `/mnt/SDCARD/Images/<system_id>` using the same
  lookup order as the frontend.
- If any `png`, `jpg`, `jpeg`, or `webp` thumbnail exists, return `exists`.
- `exists` does not compute CRCs, read DAT indexes, fetch thumbnail indexes, or
  download anything.
- Preserve subdirectory thumbnails before flat fallback thumbnails.
- Scraper-downloaded PNGs and user-placed images are treated the same and are
  not overwritten by default.
- Re-downloading an existing image requires an explicit `Force refresh` style
  option.
- Replacing user-provided thumbnails is a separate option and stays disabled by
  default.

Only ROMs without an existing image enter the CRC queue. During FE integration,
store scraper state under a path such as
`/mnt/SDCARD/plumos/state/frontend/artwork-scraper-state.json` with `system_id`,
relative path, size, mtime, ctime, kind, CRC, status, and `checked_at`. A ROM
that was previously `no_match` with the same size/mtime/ctime should be
re-CRC'd only after the DAT cache changes or when the user chooses
`Retry missing thumbnails`.
`download_failed` can be retried after a short backoff or by manual retry
because it may be a temporary network failure.
The prototype script's negative cache is keyed by `system_id`, kind, relative
path, size, mtime, and ctime instead of CRC, so unchanged unmatched ROMs can
return `negative_cached` before CRC work. Replacing a ROM with the same name and
size normally changes mtime or ctime, so it becomes eligible for CRC again.
`Retry missing thumbnails` ignores the negative cache for unusual copy paths
that preserve timestamps completely.

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
- If a CRC is absent from the libretro base DB but present in a
  build/prefetch-generated rescue overlay, the scraper uses that overlay's
  `crc -> thumbnail href` mapping.
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

`package/frontend/plumos/config/frontend/systems.json` is the source of truth
for scraper eligibility. Initial scraper targets use `enabled=true` with
`reason=simple_rom_crc`. Because they are still unvalidated on-device,
`crc_workers.max` starts at `2`; per-system max values can be raised after A30
benchmarking.
`scraper.extensions` lists the extensions the initial scraper may CRC. Even when
the system-wide `extensions` include `7z` or disk images, extensions omitted from
this scraper list are not scraped.

Enabled-system example:

```json
{
  "id": "nes",
  "scraper": {
    "enabled": true,
    "reason": "simple_rom_crc",
    "extensions": ["nes", "unf", "unif", "zip"],
    "crc_workers": { "default": 1, "bulk": 2, "max": 2 },
    "download_workers": { "default": 2, "bulk": 3, "max": 4 }
  }
}
```

Excluded systems use `scraper.enabled=false` with these reason values:

```text
cd_media_crc_expensive
pc_disk_image_policy_pending
arcade_romset_policy_pending
not_single_rom_crc
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

## Device Runner

`package/frontend/plumos/bin/plumos-thumbnail-scraper` is the packaged runner
used to validate scraper policy and existing-thumbnail skip counts on the
device, and it can also be launched from FE Apps. `--plan` / `--dry-run` does
not compute CRCs or access the network. `--fetch` processes only ROMs without an
existing thumbnail and runs CRC -> DAT lookup -> thumbnail index lookup -> PNG
download. With `--replace-existing`, ROMs that already have an image are queued
too, and the current PNG path is replaced after a successful download. The
runner does not print ROM filenames to stdout or logs; output is system-level
aggregates.

Common commands:

```sh
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --list-systems
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --system gb
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --all --limit 50
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --system gb --limit 20
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --all --limit 20
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --system gb --kind Named_Titles --replace-existing
```

Environment:

- `PLUMOS_SDCARD_ROOT`: default `/mnt/SDCARD`
- `PLUMOS_ROOT`: default `$PLUMOS_SDCARD_ROOT/plumos`
- `PLUMOS_SYSTEMS_JSON`: default `$PLUMOS_ROOT/config/frontend/systems.json`
- `PLUMOS_SCRAPER_SOURCES`: default `$PLUMOS_ROOT/config/frontend/scraper-sources.tsv`
- `PLUMOS_ROM_ROOT`: when set, scan only this root. Otherwise scan both
  `/mnt/SDCARD/Roms` and `/mnt/SDCARD/roms`, matching the frontend scanner
- `PLUMOS_IMAGE_ROOT`: default `$PLUMOS_SDCARD_ROOT/Images`
- `PLUMOS_THUMBNAIL_STATE_DIR`: default `$PLUMOS_ROOT/state/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_CACHE_DIR`: default `$PLUMOS_ROOT/cache/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_PRELOAD_DIR`: default `$PLUMOS_ROOT/share/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_FETCH_TIMEOUT`: default `45` seconds
- `PLUMOS_THUMBNAIL_FETCH_RETRY`: default `2` retries

`--system <id>` prints the reason and exits with status `2` for disabled
systems. `--all` plans enabled systems only. Output is TSV and does not print
ROM filenames.

Plan columns:

```text
status system enabled reason aliases_seen rom_candidates existing_thumbnails missing_thumbnails crc_workers download_workers
```

`rom_candidates` counts files matching `scraper.extensions` from `systems.json`.
macOS AppleDouble (`._*`), `.DS_Store`, `Thumbs.db`, `desktop.ini`, and files
under `__MACOSX` are ignored as ROM/thumbnail sidecars.
`existing_thumbnails` uses the same `/mnt/SDCARD/Images/<system_id>` lookup
order as the frontend, including subdirectory thumbnails before flat fallback
thumbnails. Only `missing_thumbnails` should enter the next CRC/DAT/download
queue.
The runner builds one temporary existing-image lookup per system to avoid
repeated `find` calls for every ROM. During `--fetch`, newly saved images are
added to that lookup so duplicate work is skipped within the same run.

Fetch columns:

```text
status system enabled reason aliases_seen rom_candidates existing_thumbnails missing_thumbnails crc_checked crc_matched downloaded no_match crc_miss thumbnail_miss download_failed invalid_png skipped_zip skipped_tool
```

`--fetch` prepends `PLUMOS_ROOT/bin` to `PATH` and uses plumOS BusyBox applets
such as `crc32`, `wget`, and `unzip`. DAT/index lookup first checks
`PLUMOS_THUMBNAIL_PRELOAD_DIR`; missing indexes are fetched into
`PLUMOS_THUMBNAIL_CACHE_DIR`. Thumbnail PNGs are downloaded from the
system-specific repositories under
`https://raw.githubusercontent.com/libretro-thumbnails/`. If GitHub raw returns
a small `.png` reference file instead of PNG bytes, the runner resolves that
reference in the same directory and fetches it again. The old
`https://thumbnails.libretro.com/` PNG fallback is disabled by default to avoid
slow server waits; set `PLUMOS_THUMBNAIL_SERVER_FALLBACK=1` to enable it.
Images are saved to `/mnt/SDCARD/Images/<system_id>/<relative stem>.png`.
For ZIP ROMs, the runner parses BusyBox `unzip -lq` output in `Length Date Time Name`
format and CRCs the first supported ROM payload member. Japanese outer ZIP filenames
are supported. If the ZIP member name is rounded to `????` by the terminal/BusyBox
path and named-member `unzip -p` returns empty output, the runner falls back to
plain `unzip -p` only when the ZIP contains exactly one regular ROM payload file.
ZIPs with sidecars or multiple payloads are left as `skipped_zip` to avoid ambiguous
CRC input. If a `.gb` payload is mixed into `Roms/gbc`, that ROM is matched against
the GB DAT/thumbnail source while the saved image still follows the selected system,
for example `/mnt/SDCARD/Images/gbc/<zip stem>.png`. This is a rescue path for mixed
ROMs; the normal recommendation is to keep ROMs in the correct system directory.
When an NES payload starts with the `NES\x1A` iNES signature, the runner strips
the 16-byte iNES header before computing the CRC32 used for libretro No-Intro
DAT lookup. Headerless payloads are CRC'd as-is.
Even for `--fetch --all`, systems without ROM candidates, or systems whose
existing thumbnails already cover all candidates, do not fetch DAT/index data.
The runner prepares DAT/index data only after it sees the first missing
thumbnail for that system.
`no_match` is the sum of `crc_miss + thumbnail_miss`. `crc_miss` means the ROM
CRC was not present in the DAT index; `thumbnail_miss` means the CRC matched but
the thumbnail index had no PNG with that canonical name. Some libretro DAT
canonical names include language tags such as `(En)` while the thumbnail index
stores only the regional name. In that case the runner builds conservative
candidates from the CRC-confirmed canonical name, dropping language and selected
auxiliary tags only, and saves the image only when the candidate still fully
matches the thumbnail index.
When a CRC is absent from the libretro base DAT, the runner also checks the
prefetch-generated rescue overlay. Rescue overlays are generated per kind from
verified seeds in `package/frontend/plumos/config/frontend/scraper-rescue-seeds.tsv`
and installed under
`/mnt/SDCARD/plumos/share/frontend/artwork-scraper/rescue/<system>/<kind>.tsv`.
The runtime never fetches external DAT files; it reads only these reduced TSVs.
For NES, CRCs derived from the NesterJ_AoEX_R3 `famicom.dat` are seeded only
when they do not duplicate the libretro base DB and their release date plus
publisher resolves to exactly one libretro thumbnail label. Ambiguous matches
are omitted to avoid downloading a wrong image.
When pre-downloaded external DATs should be used at build/prefetch time, pass a
local root with `PLUMOS_RESCUE_DAT_ROOTS` or
`scripts/prefetch-thumbnail-scraper-cache.sh --rescue-dat-root`.
Data provenance and attribution live in `docs/third-party-data.md`.
Thumbnail indexes still come from the `https://thumbnails.libretro.com/`
directory index, while PNG bytes use GitHub raw by default.
`wget` / `curl` use `PLUMOS_THUMBNAIL_FETCH_TIMEOUT` so network waits do not
stall the UI path indefinitely.

FE START -> Apps exposes a `Scraping` entry. The `Scraping` screen lets users
choose `Image < Box Art | Title Screen >`, `Existing < Replace | Skip >`, and
`System < ALL >` or a concrete system such as `System < NES >`, then start
scraping with A. Targets are limited to systems with `scraper.enabled=true` and
at least one current ROM. Opening the screen runs a library scan so ROM list and
Graphic ROM counts, and the scraping target list, update without restarting the
frontend. While running, the UI shows `Scraping Running`; internally it runs
`plumos-thumbnail-scraper --kind <kind> --system <id>` and
`plumos-thumbnail-scraper --fetch --kind <kind> --system <id>` for each target
system. When `Existing < Replace >` is selected, both commands also receive
`--replace-existing`, replacing the shared
`/mnt/SDCARD/Images/<system>/<rom>.png` path used by box art and title screens.
The RUNNING screen shows `Progress: current / total`,
`Phase: Plan|Fetch <system>`, and `Saved / NoMatch / Failed`. Planning reports
system/phase progress; fetching sets `PLUMOS_THUMBNAIL_PROGRESS=1` so scraper
`progress` rows can update ROM-level progress. Fetches started from the FE use
`PLUMOS_THUMBNAIL_FETCH_TIMEOUT=12` and `PLUMOS_THUMBNAIL_FETCH_RETRY=0` by
default so one slow image URL cannot stall the UI for a long time. It then
automatically opens `Scraping Results`.
`Scraping Results` reads only the latest
`frontend-apps-latest.log`, so users can tell the result belongs to the run they
just started. Detailed logs are still written to
`/mnt/SDCARD/plumos/logs/frontend-apps.log` and the runner log.

- `Scraping`: system selection, plan -> fetch execution, and latest results.
- `Scraping Results`: summarizes the latest log as readable multi-line rows.
  `Thumbnail Plan` shows `reason`, `aliases seen`, `ROMs`, `existing`,
  `missing`, `CRC workers`, and `DL workers`. `Fetch Thumbnails` shows
  `reason`, `aliases seen`, `ROMs`, `existing`, `missing`, `CRC checked`,
  `CRC matched`, `downloaded`, `no match`, `CRC miss`, `image miss`,
  `download failed`, `invalid PNG`, `skipped zip`, and `skipped tool`.
  The screen scrolls one row with up/down and one page with left/right.
  Field meanings and recovery notes live in `docs/thumbnail-scraping-results.en.md`.

Source definitions live in
`package/frontend/plumos/config/frontend/scraper-sources.tsv`. Columns are
`system_id`, `libretro_playlist`, and `libretro_dat_path`. Only systems with
`scraper.enabled=true` in `systems.json` are prefetch/fetch targets.

## Preloaded Cache

`scripts/prefetch-thumbnail-scraper-cache.sh` is the host/build prefetch
entrypoint. It converts libretro DAT files into `crc -> canonical game name`
TSV indexes and thumbnail directory indexes into
`canonical game name -> encoded png href` TSV indexes. Generated data is not
stored in git; frontend builds place it under
`dist/plumos-frontend/plumos/share/frontend/artwork-scraper/`.

```sh
scripts/prefetch-thumbnail-scraper-cache.sh \
  --output dist/plumos-frontend/plumos/share/frontend/artwork-scraper
```

`docker/plumos-toolchain/scripts/build-frontend.sh` defaults
`PLUMOS_PREFETCH_THUMBNAIL_CACHE=auto`, so it creates this cache when network
access is available. Set it to `0` / `false` / `skip` to skip prefetching, or
`1` / `true` to make prefetch failure a build error.

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

The FE can select `--kind Named_Boxarts` or `--kind Named_Titles`. The runner
also accepts `--kind Named_Snaps`, but that option is not exposed in the FE.
Runs started from the FE show `image`, `kind`, and `existing` at the top of
Scraping Results so the visible selection can be compared with the actual runner
options. Changing the image kind does not change the existing-image mode.

## Matching Order

1. Look for an existing thumbnail using the same order as the frontend. If one
   exists, return `exists`.
2. If no thumbnail exists and an unchanged size/mtime/ctime negative-cache entry
   exists, return `negative_cached`.
3. Compute CRC32 for the ROM payload.
4. Look up `crc -> canonical game name` in the per-system DAT index.
5. When CRC matches, derive the URL from the canonical game name and try a
   direct download. If the exact URL is absent, try exact thumbnail-index matches
   for candidates made only by dropping language and selected auxiliary tags from
   the canonical name.
6. When the base DAT has no CRC match, try the rescue overlay's
   `crc -> thumbnail href` mapping.
7. Save the image when the download succeeds. Return `download_failed` or
   `invalid_png` when it fails.
8. When neither the base DAT nor the rescue overlay matches, default behavior
   returns `no_match`.
9. Only when an explicit option is passed, try ROM-file-stem `name-exact` or
   `candidate-report` lookup.
10. Only when `--loose-index` is passed, fetch the large thumbnail directory
   index and try normalized matching as `crc-loose` or `name-loose`.
11. If all matching fails, return `no_match`. Negative cache is updated only in
   `--fetch` mode.

Dry-run does not update the negative cache.

Because `exists` and `negative_cached` return before CRC calculation, the report
CRC column can be `-` for those statuses.

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

- Review the `crc_miss` / `thumbnail_miss` split to confirm URL resolution
  rates per system.
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
