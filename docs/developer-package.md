# plumOS Developer Package

This document defines the developer package used to reproduce the plumOS A30
Docker toolchain in another checkout or future development environment.

Japanese counterpart: [developer-package.ja.md](developer-package.ja.md)

## Purpose

`dist/plumos-dev-package` is a source and toolchain snapshot for rebuilding the
runtime package. It is not a payload to extract directly onto an A30.

The package contains git-managed source, Docker files, build scripts, patches,
recipes, and documentation. It does not include `build/`, `dist/`,
`artifacts/`, ROMs, BIOS files, device logs, or generated binaries.

## Generation

Create the release package from a clean working tree:

```sh
./scripts/build-dev-package.py
```

Outputs:

- `dist/plumos-dev-package`
- `dist/plumos-dev-package.tar.gz`

Use `--allow-dirty` only for preview packages made during active work. Dirty
packages record `git_dirty=yes` and the dirty status in the manifest and must
not be used for a release.

## Package Layout

```text
plumos-dev-package/
  README.md
  manifest.txt
  sha256sum.txt
  source/
```

`source/` is the repository snapshot. Normal development work happens there.

```sh
cd plumos-dev-package/source
./scripts/docker-build.sh image
./scripts/docker-build.sh frontend
./scripts/docker-build.sh libretro-cores
```

To create a runtime package, build the required `dist/` artifacts and then run:

```sh
./scripts/build-runtime-package.py
```

See [runtime-package.md](runtime-package.md) for runtime package details.

To create the end-user SD-root package after the runtime package exists:

```sh
./scripts/build-sdroot-package.py
```

See [sdroot-package.md](sdroot-package.md) for SD-root package details.

To create the GitHub Release asset directory after both SD-root and developer
packages exist:

```sh
./scripts/build-release-bundle.py --version <version>
```

See [release-artifacts.md](release-artifacts.md) for the release split.

## Provenance

`manifest.txt` records:

- package build time
- git commit
- git branch
- working tree dirty state
- source file count
- source byte size
- required source paths

`sha256sum.txt` lists SHA-256 hashes for regular files inside the package.

## Notes

The developer package does not include a Docker image tarball. Docker images are
large and host-sensitive, so they are rebuilt from
`source/docker/plumos-toolchain/Dockerfile`.

Use the SD-root package when you need something users can place on an SD card.
The developer package is for build reproduction and should not be extracted into
`/mnt/SDCARD/plumos`.
