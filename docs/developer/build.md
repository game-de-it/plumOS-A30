# Docker Build Guide

plumOS builds A30-targeted binaries inside Docker. The goal is to keep the host
machine from influencing compiler, library, or toolchain behavior. Dockerfiles,
build scripts, patches, hashes, and recipes are kept in git.

## Requirements

- Git
- Docker
- `7zz` or a compatible 7-Zip tool
- SSH access to an A30 for device validation

Default A30 target:

```text
root@192.168.10.165
port 2222
```

Override the target when needed:

```sh
A30_TARGET=root@192.168.10.165 A30_SSH_PORT=2222 ./scripts/run-a30.sh 'uname -a'
```

## 1. Build the Docker Image

```sh
./scripts/docker-build.sh image
```

Most later build commands automatically create the image if it is missing.

## 2. Smoke Build

```sh
./scripts/docker-build.sh smoke
```

Output:

```text
dist/docker-smoke/
```

Deploy and run on the A30:

```sh
./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

## 3. Build Core Runtime Pieces

```sh
./scripts/docker-build.sh userland
./scripts/build-bootstrap-package.sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh joystickd
./scripts/docker-build.sh network-services
./scripts/build-ssh-kit.sh
```

Main outputs:

```text
dist/plumos-userland/
dist/plumos-bootstrap/
dist/plumos-frontend/
dist/plumos-joystickd/
dist/plumos-network-services/
dist/plumos-a30-ssh-kit/
```

## 4. Build Display, Python, and Apps

```sh
./scripts/docker-build.sh sdl2-runtime
./scripts/docker-build.sh python-runtime
./scripts/docker-build.sh pyxel-a30
./scripts/docker-build.sh nextcommander
./scripts/docker-build.sh music-player
```

Optional probes:

```sh
./scripts/docker-build.sh runtime-probe
./scripts/docker-build.sh mali-egl-probe
./scripts/docker-build.sh sdl2-probe
```

## 5. Build RetroArch and libretro Cores

```sh
./scripts/docker-build.sh retroarch-practical
./scripts/docker-build.sh libretro-cores
```

The default `PLUMOS_CORE_FILTER=plumos` builds the normal plumOS target set.
To build every candidate:

```sh
PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 ./scripts/docker-build.sh libretro-cores
```

Important environment variables:

| Variable | Purpose |
| --- | --- |
| `PLUMOS_CORE_FILTER` | `plumos`, `onion`, `all`, or specific core IDs |
| `LIBRETRO_CORE_BUILD_CONCURRENCY` | Number of cores built concurrently. Default: `4` |
| `JOBS` | Per-core parallel build job count |
| `LIBRETRO_OPTIMIZATION_PROFILE` | `speed`, `compat`, `aggressive`, `size`, `debug` |
| `FAIL_ON_CORE_ERROR` | `1` makes any core failure fail the command |

Core source/ref/build options are tracked in:

```text
docker/plumos-toolchain/libretro-core-recipes.tsv
docs/onion-libretro-source-lock.tsv
docs/libretro-core-version-inventory.tsv
```

## 6. Build PicoArch

```sh
./scripts/docker-build.sh picoarch
```

Output:

```text
dist/plumos-picoarch/
```

PicoArch generally shares the libretro cores under
`plumos/retroarch/cores/`.

## 7. Build Standalone Emulators

Build the adopted standalone emulator set:

```sh
PLUMOS_STANDALONE_FILTER=ppsspp,scummvm,easyrpg,pcsx_rearmed,openbor \
FAIL_ON_STANDALONE_ERROR=1 \
TARGET_DIR=/workspace/dist/plumos-standalone-emulators-adopted \
./scripts/docker-build.sh standalone-emulators
```

Build one emulator for testing:

```sh
PLUMOS_STANDALONE_FILTER=ppsspp ./scripts/docker-build.sh standalone-emulators
```

## 8. Build the Runtime Package

```sh
./scripts/build-runtime-package.py
```

Output:

```text
dist/plumos-runtime-package/
dist/plumos-runtime-package.tar.gz
```

The script checks `docs/emulator-runtime-manifest.tsv` and verifies that required
launchers, binaries, and cores are present.

## 9. Build or Stage the SD Root Package

Script-generated package:

```sh
./scripts/build-sdroot-package.py
```

Output:

```text
dist/plumos-sdroot-package/
dist/plumos-sdroot-package.tar.gz
```

Current end-user releases use the validated SD-root staging directory from a
known-good SD card:

```sh
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
rm -f dist/plumos-sdroot-package.7z
cd dist/plumos-release-sdroot
COPYFILE_DISABLE=1 7zz a -t7z -mx=5 -mmt=on ../plumos-sdroot-package.7z .
cd ../..
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

The `.7z` archive expands directly into the SD-card root and does not contain an
outer package directory.

## 10. Deploy to the A30

```sh
./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

`deploy-a30.sh` preserves mutable config by default. Use
`PLUMOS_DEPLOY_PROTECT_CONFIG=0` only when intentionally resetting settings.

Use the FE control helper for frontend state:

```sh
./scripts/a30-fe-control.sh restart
./scripts/a30-fe-control.sh status
```

`status` should report exactly one `/dev/fb0` owner.

## 11. Run Commands and Collect Logs

```sh
./scripts/run-a30.sh 'free -m'
./scripts/run-a30.sh 'ps | grep plumOS'
./scripts/collect-a30-logs.sh
```

## 12. Common Probes

Frontend:

```sh
./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
```

SDL2 gamepad:

```sh
./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

RetroArch minimal:

```sh
./scripts/docker-build.sh retroarch-minimal
./scripts/probe-a30-retroarch-minimal.sh --deploy --duration 10 --rotation ccw
```

Mali EGL:

```sh
./scripts/docker-build.sh mali-egl-probe
./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

## 13. Release Checklist

```sh
git status --short
./scripts/audit-release-sdroot.py dist/plumos-release-sdroot
7zz t dist/plumos-sdroot-package.7z
shasum -a 256 dist/plumos-sdroot-package.7z
```

Then extract the SD-root archive to a fresh SD card and boot it on real
hardware.

## Detailed References

- [Docker Toolchain Details](../docker-toolchain.md)
- [Runtime Package](../runtime-package.md)
- [SD Root Package](../sdroot-package.md)
- [Release Artifacts](../release-artifacts.md)
- [Emulator Runtime Manifest](../emulator-runtime-manifest.md)

## Japanese Counterpart

- [Japanese Docker build guide](build.ja.md)
