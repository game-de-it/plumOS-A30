# plumOS A30

<p align="center">
  <img src="docs/assets/screenshots/plumos-a30-hero.png" alt="plumOS running on a Miyoo A30" width="720">
</p>

plumOS is an SD-card based custom environment for the Miyoo A30. It avoids
modifying the device rootfs/NAND whenever possible and keeps the plumOS
frontend, launchers, emulator runtime, configuration, logs, and helper tools
under `plumos/` on the SD card.

End users extract the release archive to the root of a formatted SD card.
Developers rebuild the A30 runtime, RetroArch/libretro cores, PicoArch,
standalone emulators, and frontend through the Docker toolchain.

English `.md` documents are the default GitHub-facing documentation. Japanese
counterparts use the same path with a `.ja.md` suffix.

## Documentation

- [User guide](docs/user/README.md)
- [Developer guide](docs/developer/README.md)
- [Developer feature reverse lookup](docs/developer/feature-index.md)
- [Documentation index](docs/README.md)
- [Japanese README](README.ja.md)
- [Japanese documentation index](docs/README.ja.md)
- [TODO](TODO.md)

## End-user package

The current end-user package is a `.7z` archive that expands directly into the
SD card root.

```text
dist/plumos-sdroot-package.7z
```

Expected top-level entries after extraction:

```text
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

ROMs, BIOS files, save/state data, screenshots, videos, network secrets, and
personal SSH keys are not included.

## Development Loop

Build the Docker image:

```sh
./scripts/docker-build.sh image
```

Build selected components:

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh retroarch-practical
./scripts/docker-build.sh libretro-cores
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone-emulators
```

Run commands and deploy to an A30 through the helper scripts:

```sh
./scripts/run-a30.sh 'uname -a'
./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
./scripts/a30-fe-control.sh restart
./scripts/a30-fe-control.sh status
```

See [Docker build guide](docs/developer/build.md) for the full workflow.

## Core Policies

- Do not modify the A30 rootfs/NAND as the normal path.
- Keep plumOS-owned persistent files under `/mnt/SDCARD/plumos/`.
- Ship the StockOS SD payload needed for fallback and compatibility.
- Build libretro cores from source. Prefer the Onion source period for cores that Onion carries.
- Users provide their own ROMs and BIOS files.
