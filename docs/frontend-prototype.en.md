# Frontend Prototype

The first plumOS frontend is a compatibility scanner before any real rendering
work. It runs on the A30 and lists stock SD-card configuration from `Emu`,
`RApp`, `App`, and `Themes`.

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
- Existence check for `/mnt/SDCARD/Roms/recentlist.json`

Fields currently read:

- `label`
- `name`
- `launch`
- `rompath`
- `extlist`

## Device Check

Manual mode was run on the A30 on 2026-06-06 and confirmed:

```text
summary emu=18 rapp=27 app=5 themes=42
```

## Next Additions

- Enumerate ROM files from ROM directories.
- Add `imgpath` and artwork lookup.
- Read `launchlist`.
- Handle `gamelist` XML.
- Parse/update `recentlist.json`.
- Build the first SDL/input UI.
