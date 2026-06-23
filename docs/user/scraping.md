# ROM Thumbnail Scraping

plumOS can download ROM thumbnails for supported systems from libretro database
and thumbnail indexes. The feature is exposed from the frontend through
`START -> Apps -> Scraping`.

Japanese counterpart: [scraping.ja.md](scraping.ja.md)

## What It Does

The scraper checks ROM files by CRC32, matches them against the preloaded
libretro DAT indexes, and saves PNG thumbnails under `Images/`.

The normal output path follows the ROM directory name that contains the game:

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png

Roms/SFC/Akumajou Densetsu.sfc
Images/SFC/Akumajou Densetsu.png
```

`Imgs/` is kept only for StockOS-compatible or older artwork. New plumOS
thumbnail data should use `Images/`.

## Frontend Flow

1. Open `START -> Apps -> Scraping`.
2. Choose the image type.
   - `Box Art`
   - `Title Screen`
3. Choose whether existing images are skipped or replaced.
4. Choose `ALL` or one supported system.
5. Press A to run scraping.
6. Review the result screen after the run finishes.

Running from the frontend scans the ROM library first, so newly added ROMs can
be picked up without restarting the frontend.

## Existing Images

By default, existing thumbnails are skipped. This protects images that the user
placed manually.

When `Replace` is selected, the scraper may overwrite the existing PNG path
after a successful download. ROMs, saves, BIOS files, and emulator settings are
not modified by scraping.

## Supported Systems

This table reflects `scraper.enabled=true` in
`plumos/config/frontend/systems.json`. The matching source is
`plumos/config/frontend/scraper-sources.tsv`.

| system_id | Display Name | Common ROM Directories | Scraped Extensions | CRC Workers | Download Workers |
| --- | --- | --- | --- | --- | --- |
| `nes` | NES | `FC, nes, famicom` | `nes, unf, unif, zip` | `8` | `6` |
| `fds` | FDS | `FDS, FC, fds` | `fds, zip` | `8` | `6` |
| `sfc` | SFC | `SFC, sfc, snes` | `sfc, smc, fig, bs, swc, zip` | `8` | `6` |
| `gb` | GB | `GB, gb` | `gb, zip` | `8` | `6` |
| `gbc` | GBC | `GBC, GB, gbc` | `gbc, zip` | `8` | `6` |
| `gba` | GBA | `GBA, gba` | `gba, zip` | `4` | `4` |
| `megadrive` | Mega Drive | `MD, megadrive, genesis` | `gen, md, smd, bin, zip` | `8` | `6` |
| `mastersystem` | Master System | `MS, MD, mastersystem` | `sms, bin, zip` | `8` | `6` |
| `gamegear` | Game Gear | `GG, GameGear, MD, gamegear` | `gg, zip` | `8` | `6` |
| `sega32x` | 32X | `THIRTYTWOX, MD, sega32x` | `32x, bin, zip` | `8` | `6` |
| `pcengine` | PC Engine | `PCE, pcengine, tg16` | `pce, sgx, zip` | `8` | `6` |
| `supergrafx` | SuperGrafx | `SGX, PCE, supergrafx` | `sgx, pce, zip` | `8` | `6` |
| `ngp` | NGP | `NGP, ngp` | `ngp, zip` | `8` | `6` |
| `ngpc` | NGPC | `NGPC, NGP, ngpc` | `ngc, ngpc, zip` | `8` | `6` |
| `wonderswan` | WonderSwan | `WS, WSC, wonderswan` | `ws, zip` | `8` | `6` |
| `wonderswancolor` | WonderSwan Color | `WSC, wonderswancolor` | `wsc, zip` | `8` | `6` |
| `lynx` | Atari Lynx | `LYNX, lynx` | `lnx, lyx, zip` | `8` | `6` |
| `virtualboy` | Virtual Boy | `VB, virtualboy` | `vb, vboy, bin, zip` | `8` | `6` |
| `msx` | MSX | `MSX, msx` | `rom, mx1, mx2, zip` | `8` | `6` |
| `atari2600` | Atari 2600 | `ATARI2600, atari2600` | `a26, bin, zip` | `8` | `6` |
| `atari7800` | Atari 7800 | `ATARI7800, atari7800` | `a78, bin, zip` | `8` | `6` |
| `vectrex` | Vectrex | `VECTREX, vectrex` | `vec, bin, zip` | `8` | `6` |
| `supervision` | Supervision | `SUPERVISION, supervision` | `sv, bin, zip` | `8` | `6` |
| `odyssey2` | Odyssey2 | `ODYSSEY2, VIDEOPAC, odyssey2` | `bin, zip` | `8` | `6` |
| `pokemini` | Pokemon Mini | `POKEMINI, pokemini` | `min, zip` | `8` | `6` |
| `atari5200` | Atari 5200 | `FIFTYTWOHUNDRED, atari5200` | `a52, bin, zip` | `8` | `6` |
| `channelf` | Fairchild Channel F | `FAIRCHILD, channelf` | `bin, chf, rom, zip` | `8` | `6` |
| `colecovision` | ColecoVision | `COLECO, coleco` | `rom, ri, mx1, mx2, col, sg, sc, zip` | `8` | `6` |
| `intellivision` | Intellivision | `INTELLIVISION, intellivision` | `int, bin, rom, zip` | `8` | `6` |
| `sg1000` | SG-1000 | `SEGASGONE, sg-1000` | `sg, mv, bin, rom, sms, gg` | `8` | `6` |

## Unsupported Systems

Unsupported systems can still display manually placed thumbnails under
`Images/<ROM directory>/`. They are only excluded from automatic scraping.

Common exclusion reasons are:

- CD or disc-image matching would require a representative file policy.
- PC disk images need per-format DAT and image-selection rules.
- Arcade and romset systems depend on internal ROM members and romset versions.
- Apps, engines, and contentless cores are not single-ROM CRC targets.
- Some systems are disabled in the frontend or still lack a scraper source.

## Command Line

The same runner can be used from SSH:

```sh
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --list-systems
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --system gba
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --system gba
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --all
```

The command output is system-level TSV. It does not print ROM filenames.

## Logs and Results

Frontend runs write detailed logs under `plumos/logs/`. The on-device
`Scraping Results` screen summarizes the latest run.

See [Thumbnail Scraping Results](../thumbnail-scraping-results.md) for the
meaning of result fields such as `downloaded`, `no_match`, `crc_miss`, and
`download_failed`.
