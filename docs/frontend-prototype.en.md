# Frontend Prototype

The first plumOS frontend is a compatibility scanner before any real rendering
work. It runs on the A30, reads stock SD-card configuration from `Emu`, `RApp`,
`App`, and `Themes`, and checks the referenced ROM/artwork/metadata paths.

## Build

```sh
./scripts/docker-build.sh frontend
```

Outputs:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

## Deploy

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

## Manual Run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

Manual mode exits with `0`. Normal mode, as launched by the wrapper, currently
exits with `75` so the wrapper falls back to stock MainUI.
Normal mode suppresses stdout and writes details to
`/mnt/SDCARD/plumos/logs/plumos-frontend.log`. Set `PLUMOS_FRONTEND_STDOUT=1`
to also print to stdout.

## Current Inputs

- `/mnt/SDCARD/Emu/*/config.json`
- `/mnt/SDCARD/RApp/*/config.json`
- `/mnt/SDCARD/App/*/config.json`
- `/mnt/SDCARD/Themes/*/config.json`
- Existence, regular-file count, and `extlist` match count for `rompath`
  directories.
- Existence and image-file count for `imgpath` artwork directories.
- Existence and size for `gamelist` files.
- `launchlist` entry count.
- Existence, size, and non-empty-line entry count for
  `/mnt/SDCARD/Roms/recentlist.json`

Fields currently read:

- `label`
- `name`
- `launch`
- `rompath`
- `imgpath`
- `extlist`
- `gamelist`
- `launchlist`

`rompath`, `imgpath`, and `gamelist` are resolved relative to the directory that
contains each `config.json`. `extlist` matching is case-insensitive. The scanner
does not log ROM/artwork filenames yet; it only records counts.

## Device Check

Manual mode was run on the A30 on 2026-06-06 and confirmed:

```text
summary configs emu=18 rapp=27 app=5 themes=42
summary roms dirs=25 files=0 matched=0
summary artwork dirs=41 images=4027
summary metadata gamelists=0 launchers=22
```

`Roms/recentlist.json` was detected as `size=234 entries=3`. The scanner does
not log the values; it only treats non-empty lines as entries for now.

Observations:

- On the current SD card, many configured ROM directories exist, but ROM files
  are almost empty.
- `Imgs` contains existing artwork, so plumOS should be able to reference
  artwork without moving it.
- `RApp/mednafen_wswan/config.json` has `imgpath` set to `../..Imgs/WSC`, which
  appears to be a typo for `../../Imgs/WSC`. Stock compatibility should detect
  and optionally correct cases like this.

## Next Additions

- Match ROM filenames to artwork filenames.
- Parse `gamelist` XML.
- Parse/update `recentlist.json`.
- Build the first SDL/input UI.
