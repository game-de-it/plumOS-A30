# plumOS SD-Root Package

This document defines the SD-card root package distributed to end users.

Japanese counterpart: [sdroot-package.ja.md](sdroot-package.ja.md)

## Purpose

`dist/plumos-release-sdroot` or `dist/plumos-sdroot-package` is the finished
contents of `/mnt/SDCARD` for distribution.

Users extract the archive to the root of a formatted SD card. The StockOS boot
flow then starts the plumOS frontend. plumOS is distributed as an overlay on top
of the stock SD payload; it is not independent of stock SD-side files such as
`miyoo/`.

## Build

The SD-root package is built from the runtime package and stock SD payload:

```sh
./scripts/build-runtime-package.py
./scripts/build-sdroot-package.py
```

Default stock payload input:

```text
artifacts/stock-sdl-probe/extracted/mnt/SDCARD
```

Before a formal release, refresh this input from a known-good stock SD card with
ROMs, BIOS files, saves, media, and user data removed.

Script outputs:

- `dist/plumos-sdroot-package`
- `dist/plumos-sdroot-package.tar.gz`

The current end-user asset is made by auditing the known-good
`dist/plumos-release-sdroot` directory and archiving it as a `.7z` without an
outer package directory:

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
rm -f dist/plumos-sdroot-package.7z
cd dist/plumos-release-sdroot
COPYFILE_DISABLE=1 7zz a -t7z -mx=5 -mmt=on ../plumos-sdroot-package.7z .
cd ../..
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

The `.7z` format is chosen so users only need one extraction step.

After extraction, the destination root directly contains entries such as:

```text
miyoo/app/MainUI
miyoo/app/MainUI.stock
miyoo/app/keymon
miyoo/app/sdlloading
miyoo/lib/
Emu/
RetroArch/
plumos/
Bios/
Images/
Imgs/
Roms/
Saves/
README.txt
manifest.txt
sha256sum.txt
```

Run the release preflight before archiving any SD-root staging directory:

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
```

If blockers remain, move only clear generated files, history, saves, or backups
into quarantine and recheck:

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot --clean
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
```

Warnings are not deleted automatically. Stock emulator BIOS/system ROMs,
legacy `Imgs/` artwork, and `plumos/ssh/etc/authorized_keys` need explicit
release-policy decisions.

## Included

- stock SD payload
  - `miyoo/app/MainUI.stock`: stock MainUI fallback
  - `miyoo/app/keymon`, `miyoo/app/sdlloading`
  - `miyoo/lib/`: stock SDL1/SDL2 and related libraries
  - non-user runtime trees from stock payload, such as `Emu/` and `RetroArch/`
- `miyoo/app/MainUI`: plumOS `MainUI.wrapper`; stock `MainUI` is preserved as
  `MainUI.stock`
- `plumos/`: frontend, runtime, emulators, cores, default config, helpers
- `plumos/state/standalone/ppsspp/`: PPSSPP factory state for a fresh SD card
- `plumos/ssh/`: Dropbear SSH, start/stop scripts, `password.hash`, and optional
  `authorized_keys`
- placeholder `Bios/`, `Images/`, `Imgs/`, `Roms/`, and `Saves/`
  - normal plumOS thumbnails live in `Images/<system_id>/`
  - `Imgs/` remains for StockOS artwork compatibility and older data
- `README.txt`
- `manifest.txt`
- `sha256sum.txt`

## Excluded

- ROMs
- BIOS files
- saves/states
- screenshots/videos
- top-level `.config/` user or runtime state
- network secrets
- user logs
- build cache

## Notes

This package is primarily for fresh/formatted SD cards. If extracting over an
existing StockOS SD card, back up `miyoo/app/MainUI` first.

For safe updates to an existing SD card, use the runtime installer described in
[runtime-package.md](runtime-package.md).

## Release Checks

Before a formal release, verify:

- `miyoo/app/MainUI` is an executable wrapper.
- `plumos/bootstrap/MainUI.wrapper` and `miyoo/app/MainUI` have identical
  content.
- `miyoo/app/MainUI.stock`, `miyoo/app/keymon`, and `miyoo/app/sdlloading`
  exist.
- `miyoo/lib/libSDL2-2.0.so.0` exists.
- `plumos/bin/plumos-controller-ui-mali` exists.
- `plumos/config/frontend/systems.json` exists.
- `plumos/ssh/start-ssh.sh` and `plumos/ssh/bin/dropbear` exist.
- `plumos/ssh/etc/password.hash` exists and password login with `plumos` works.
- `plumos/ssh/etc/authorized_keys` is optional; if present, key auth also works.
- `Roms/`, `Bios/`, `Images/`, and `Imgs/` contain no user data except
  placeholders.
- Extracting the `.7z` into an empty directory creates an SD-card-root layout.
- A fresh-SD-equivalent device boots successfully.
