# Thumbnail Scraping Results

This document explains the fields shown on the plumOS `Scraping Results`
screen and what users can do when failures appear.

`Scraping Results` shows only the latest scraping run. Use up/down to scroll
one row and left/right to move one page.

## Plan Result

`Thumbnail Plan` counts ROMs and already available thumbnails before any
download work starts.

```text
nes
reason simple_rom_crc
aliases seen 1
ROMs 50
existing 8
missing 42
CRC workers 1/2/2
DL workers 2/3/4
----------------
```

- `reason`: why this system is eligible for scraping.
- `aliases seen`: ROM directory aliases found for the system.
- `ROMs`: ROM candidates found for scraping.
- `existing`: ROMs that already have an image under `/mnt/SDCARD/Images/<system>/`.
- `missing`: ROMs that do not have an image yet. Fetch normally works on these.
- `CRC workers`: CRC worker counts in `default/bulk/max` order.
- `DL workers`: download worker counts in `default/bulk/max` order.

## Fetch Result

`Fetch Thumbnails` runs CRC lookup and image download for ROMs that do not
already have thumbnails.

```text
nes
reason simple_rom_crc
aliases seen 1
ROMs 50
existing 8
missing 42
CRC checked 42
CRC matched 12
downloaded 12
no match 30
CRC miss 20
image miss 10
download failed 1
invalid PNG 0
skipped zip 0
skipped tool 0
----------------
```

- `CRC checked`: ROMs whose CRC was calculated.
- `CRC matched`: ROMs whose CRC was found in the libretro DAT.
- `downloaded`: PNG files saved by this run.
- `no match`: `CRC miss` plus `image miss`.
- `CRC miss`: ROMs whose CRC was not present in the libretro DAT.
- `image miss`: ROMs whose CRC matched, but libretro thumbnails had no image.
- `download failed`: a URL was known, but network or save failed.
- `invalid PNG`: downloaded data was not usable as a PNG.
- `skipped zip`: ZIP extraction failed or no supported ROM member was found.
- `skipped tool`: required tools, DAT data, thumbnail index, or source data were missing.

## What To Do

- When `CRC miss` is high:
  the ROM dump may not match a known clean dump. Try re-dumping, use another
  verified dump, or place an image manually.
- When `image miss` is high:
  the ROM is recognized, but libretro does not have an image. Manual images are
  the reliable workaround.
- When `download failed` appears:
  check Wi-Fi, DNS, server response, and SD card writes, then retry later.
- When `invalid PNG` appears:
  the server may have returned non-image data. Retry later.
- When `skipped zip` appears:
  the ZIP has no supported ROM payload, or extraction failed. Japanese outer ZIP
  filenames are supported. If a `.gb` payload is mixed into `Roms/gbc`, the scraper
  rescues it through the GB source, but unknown extensions or broken ZIPs are skipped.
  Inspect the ZIP contents and move the ROM to the correct system directory when needed.
- When `skipped tool` appears:
  check the plumOS runtime, cache/preload data, network sources, and update
  plumOS if needed.

User-provided images can still be displayed for systems that are not scraper
targets. Place images under `/mnt/SDCARD/Images/<system>/`.
