# plumOS Runtime Package

This document defines how to build and install the plumOS runtime package for
the A30 SD card.

Japanese counterpart: [runtime-package.ja.md](runtime-package.ja.md)

## Purpose

`dist/plumos-runtime-package` is the runtime payload installed under
`/mnt/SDCARD/plumos`. It groups the frontend, launchers, RetroArch, libretro
cores, PicoArch, standalone emulators, Pyxel runtime, NextCommander file manager,
GMU/music player payloads, SDL/Mali runtime, and helper userland into one update
unit.

The normal end-user release asset is the SD-root package built from this runtime
package, currently `dist/plumos-sdroot-package.7z`. The runtime package remains
useful for safe updates to an existing SD card and as an intermediate artifact
for SD-root packaging.

ROMs, BIOS files, user saves/states, existing settings, and logs are outside the
runtime package responsibility. The installer preserves mutable paths before
overwriting the payload, then restores them.

## Input Artifacts

`scripts/build-runtime-package.py` overlays these `dist/` artifacts in order.
Later components win when the same path differs:

- `dist/plumos-userland` optional
- `dist/plumos-bootstrap`
- `dist/plumos-frontend`
- `dist/plumos-runtime-probe`
- `dist/plumos-joystickd`
- `dist/plumos-network-services`
- `dist/plumos-a30-ssh-kit`
- `dist/plumos-retroarch-practical`
- `dist/plumos-sdl2-runtime`
- `dist/plumos-python-runtime`
- `dist/plumos-pyxel-a30`
- `dist/plumos-nextcommander`
- `dist/plumos-gmu`
- `dist/plumos-libretro-cores-onion-lock-all`
- `dist/plumos-libretro-cores-mgba-fix` optional, overlaid after the all-core
  artifact when an older `onion-lock-all` artifact still contains the mGBA
  `projectName` / `anonymousMemoryMap` issue
- `dist/plumos-picoarch`
- `dist/plumos-standalone-emulators-adopted`

Build only adopted standalone emulators for normal distribution:

```sh
PLUMOS_STANDALONE_FILTER=ppsspp,scummvm,easyrpg,pcsx_rearmed,openbor \
FAIL_ON_STANDALONE_ERROR=1 \
TARGET_DIR=/workspace/dist/plumos-standalone-emulators-adopted \
./scripts/docker-build.sh standalone-emulators
```

The SSH kit is built with `scripts/build-ssh-kit.sh`. The default login path is
the password `plumos`; the hash is stored on the SD card as
`plumos/ssh/etc/password.hash`. Include an authorized key only when a test or
private package needs it:

```sh
./scripts/build-ssh-kit.sh
```

## Build

```sh
./scripts/build-runtime-package.py
```

Outputs:

- `dist/plumos-runtime-package`
- `dist/plumos-runtime-package.tar.gz`

Package metadata:

- `plumos/share/doc/plumos-runtime-package/manifest.txt`
- `plumos/share/doc/plumos-runtime-package/emulator-runtime-manifest.tsv`
- `plumos/share/doc/plumos-runtime-package/overlay-overrides.txt`
- `sha256sum.txt`

`overlay-overrides.txt` lists files where a later component replaced different
content from an earlier component. Common examples are `plumos-env` and SDL
runtime files; the final payload must keep the frontend launcher and the SDL
runtime with the `a30mali` backend.

## Install

Extract the archive at the SD-card root and run the installer:

```sh
tar -xzf plumos-runtime-package.tar.gz
cd plumos-runtime-package
./install-plumos-runtime.sh /mnt/SDCARD
```

If `/mnt/SDCARD/plumos` already exists, the installer creates a tar backup under
`/mnt/SDCARD/plumos-backups/` by default. Disable that only when appropriate:

```sh
PLUMOS_RUNTIME_BACKUP=0 ./install-plumos-runtime.sh /mnt/SDCARD
```

## Preserved Mutable Paths

The installer preserves:

- `plumos/config/frontend/settings.json`
- `plumos/config/system/settings.json`
- `plumos/config/network/settings.json`
- `plumos/config/performance/settings.json`
- `plumos/config/standalone`
- `plumos/state`
- `plumos/logs`
- `plumos/retroarch/config/retroarch-minimal.cfg`
- `plumos/retroarch/config/retroarch-practical.cfg`
- `plumos/retroarch/saves`
- `plumos/retroarch/states`
- `plumos/retroarch/playlists`

## Verification

Package creation reads `docs/emulator-runtime-manifest.tsv` and verifies that the
payload contains the launcher, core, and standalone binary required by each FE
runtime profile.

Fresh-SD install and rollback validation must be performed before release. See
[sdroot-package.md](sdroot-package.md) for the user-facing SD-root package.
