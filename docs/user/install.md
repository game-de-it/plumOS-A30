# Installation

This procedure installs plumOS to a newly formatted SD card.

## Requirements

- Miyoo A30
- microSD card
- PC that can write the SD card
- plumOS SD root package

The expected end-user archive is:

```text
plumos-sdroot-package.7z
```

## Extract to the SD Card

1. Format the SD card on your PC.
2. Extract `plumos-sdroot-package.7z` once.
3. Copy the extracted contents to the root of the SD card.
4. Confirm that `miyoo/`, `plumos/`, `Roms/`, `Bios/`, and similar entries are
   visible directly at the SD-card root.
5. Insert the SD card into the A30 and power it on.

Correct layout:

```text
SDCARD/
  App/
  Bios/
  Emu/
  Images/
  Imgs/
  RApp/
  RetroArch/
  Roms/
  Saves/
  Themes/
  miyoo/
  plumos/
```

Incorrect layout:

```text
SDCARD/
  plumos-sdroot-package/
    App/
    Bios/
    miyoo/
    plumos/
```

If the extra `plumos-sdroot-package/` directory appears, move its contents up to
the SD-card root.

## ROMs and BIOS Files

Place ROMs under `Roms/`.

```text
Roms/nes/
Roms/snes/
Roms/gba/
Roms/psx/
```

Place BIOS files under `Bios/`. Required file names and subdirectories depend on
the emulator/core. plumOS release packages do not include BIOS files.

## Thumbnails

The normal plumOS thumbnail path is `Images/<ROM directory name>/`.

Example:

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png
```

`Imgs/` is kept as a legacy StockOS-compatible artwork directory. Use `Images/`
for new plumOS thumbnails.

## SSH

Enable SSH from `Network Settings` -> `NW Service`, then connect from your PC:

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

Default password:

```text
plumos
```

SFTP uses the same SSH password. Key authentication is also available if you add
your own `authorized_keys`, but public release archives must not include personal
keys.

## Installing Over an Existing SD Card

The SD root package is primarily intended for a fresh/formatted SD card. If you
overwrite an existing card, back up ROMs, BIOS files, save/state data,
screenshots, videos, and personal settings first.

## Japanese Counterpart

- [Japanese installation guide](install.ja.md)
