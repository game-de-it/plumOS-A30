# plumOS Release Artifacts

This document defines the release artifact split for plumOS A30 and the files
uploaded to GitHub Releases.

Japanese counterpart: [release-artifacts.ja.md](release-artifacts.ja.md)

## Release Channels

plumOS uses separate end-user and developer releases.

### End-User Release

The end-user release is the SD-root package extracted directly to the A30 SD
card root.

Release asset:

- `plumos-a30-sdroot-<version>.7z`

Includes:

- stock SD payload
  - `miyoo/app/MainUI.stock`
  - `miyoo/app/keymon`, `miyoo/app/sdlloading`
  - `miyoo/lib/` stock SDL/runtime libraries
  - non-user runtime files from the stock payload
- `miyoo/app/MainUI` plumOS boot wrapper
- frontend and launchers
- RetroArch runtime and adopted libretro cores
- PicoArch runtime
- adopted standalone emulators
- Pyxel runtime
- SDL/Mali runtime
- joystick, network, and userland helpers
- fresh-SD PPSSPP factory state under `plumos/state/standalone/ppsspp`
- Dropbear SSH kit
- empty `Roms/`, `Bios/`, `Images/`, `Imgs/`, and `Saves/` placeholders
  - `Images/` is the normal plumOS thumbnail location
  - `Imgs/` remains for StockOS artwork compatibility and older data
- SD-root manifest
- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `THIRD_PARTY_NOTICES.ja.md`

Excludes:

- ROMs
- BIOS files
- user save/state data
- top-level `.config/` user or runtime state
- network secrets
- build cache
- development source tree

SSH accepts the initial password `plumos`. Its hash is stored in
`plumos/ssh/etc/password.hash`. `plumos/ssh/etc/authorized_keys` is optional and
must not contain personal keys in public archives.

`dist/plumos-runtime-package.tar.gz` can also be generated as an internal update
artifact for existing SD cards. It is installed with `install-plumos-runtime.sh`
and preserves existing settings and save/state data. It is not the primary
GitHub Release asset for normal users.

### Developer Release

The developer release is the source/toolchain package used to rebuild the
SD-root and runtime packages.

Release asset:

- `plumos-a30-developer-<version>.tar.gz`

Includes:

- git-managed source snapshot
- Dockerfile
- build scripts
- patches
- libretro/standalone build recipes
- documentation

Excludes:

- Docker image tarball
- `build/`
- `dist/`
- `artifacts/`
- ROM, BIOS, and user data

The Docker image is rebuilt from the developer package because image tarballs are
large and host-sensitive.

## Release Bundle

Create the GitHub Release asset directory with:

```sh
./scripts/build-release-bundle.py --version <version>
```

Default inputs:

- `dist/plumos-sdroot-package.7z`
- `dist/plumos-dev-package.tar.gz`

Example output:

```text
dist/plumos-release-<version>/
  plumos-a30-sdroot-<version>.7z
  plumos-a30-developer-<version>.tar.gz
  RELEASE_NOTES.md
  manifest.txt
  SHA256SUMS
```

Build release bundles from a clean working tree. Use `--allow-dirty` only for
previews, never for formal releases.

## GitHub Release Assets

GitHub Release assets are fixed to:

- `plumos-a30-sdroot-<version>.7z`
- `plumos-a30-developer-<version>.tar.gz`
- `SHA256SUMS`
- `manifest.txt`
- `RELEASE_NOTES.md`

`LICENSE` and `THIRD_PARTY_NOTICES.md` are included inside the SD-root and
developer packages rather than uploaded as separate GitHub Release assets.

The release body is based on `RELEASE_NOTES.md`.

## Build Order

1. Start from a clean working tree.
2. Build the runtime package inputs. If a package needs an SSH public key for
   connection testing, run
   `A30_AUTHORIZED_KEYS="$HOME/.ssh/id_ed25519.pub" ./scripts/build-ssh-kit.sh`.
   Public releases should omit personal keys and use password login as the
   initial path.
3. Prepare a stock payload without ROMs, BIOS files, saves, media, or user data
   under `artifacts/stock-sdl-probe/extracted/mnt/SDCARD`, or pass a path with
   `./scripts/build-sdroot-package.py --stock-sdcard-dir <path>`.
4. Run `./scripts/build-runtime-package.py`.
5. Run `./scripts/build-sdroot-package.py`.
6. If using `dist/plumos-release-sdroot` from a known-good device SD card, audit
   it before archiving:
   `./scripts/audit-release-sdroot.py dist/plumos-release-sdroot`.
   Use `--clean` only for clear generated/history/save/backup files. Create a
   `.7z` without an outer directory, then verify with `7zz t` and
   `shasum -a 256`.
7. Run `./scripts/build-dev-package.py`.
8. Run `./scripts/build-release-bundle.py --version <version>`.
9. Verify `SHA256SUMS`.
10. Validate boot on a fresh-SD-equivalent environment.
11. Validate existing-SD runtime install/rollback if needed.
12. Upload the release bundle assets to GitHub Releases.

## Done Criteria

A formal release must satisfy:

- runtime package passes the runtime/core/binary checks from
  `docs/emulator-runtime-manifest.tsv`
- SD-root package includes the stock SD payload, `miyoo/app/MainUI`, and the
  `plumos/` runtime, and excludes ROM/BIOS/user data
- SD-root package includes `LICENSE`, `THIRD_PARTY_NOTICES.md`, and
  `THIRD_PARTY_NOTICES.ja.md`
- `THIRD_PARTY_NOTICES.md` includes the Miyoo firmware page for StockOS SD
  payload reference and the NextCommander upstream/source link
- developer package manifest records `git_dirty=no`
- release bundle manifest records `git_dirty=no`
- `SHA256SUMS` verifies all assets
- fresh-SD boot validation is complete

Until fresh-SD validation is complete, treat the GitHub Release as a draft or
pre-release.
