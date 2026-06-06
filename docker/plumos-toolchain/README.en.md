# plumOS Toolchain Docker

This is the A30 build environment for plumOS. The first goal is to build armhf
artifacts inside Docker and write them to `dist/`.

## Image Build

```sh
./scripts/docker-build.sh image
```

The default image tag is `plumos-a30-toolchain:dev`. To override it:

```sh
PLUMOS_DOCKER_IMAGE=plumos-a30-toolchain:local ./scripts/docker-build.sh image
```

## Smoke Build

```sh
./scripts/docker-build.sh smoke
```

Outputs:

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

The smoke binary is statically linked with `arm-linux-gnueabihf-gcc`, so it can
be used as a small device validation binary without directly depending on the
stock A30 glibc `2.23`.

## plumOS Userland Build

```sh
./scripts/docker-build.sh userland
```

Outputs:

```text
dist/plumos-userland/plumos/bin/busybox
dist/plumos-userland/plumos/bin/plumos-env
dist/plumos-userland/plumos/share/doc/busybox/
```

BusyBox is built from the official `1.38.0` tarball after SHA-256 verification.
Since the SD card is vfat and does not handle symlinks well, applets are
installed as small wrapper scripts instead of symlinks.

Deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-userland /mnt/SDCARD
```

Run:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/plumos-env free -m'
```

## Frontend Prototype Build

```sh
./scripts/docker-build.sh frontend
```

Outputs:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/bin/plumos-library-scan
dist/plumos-frontend/plumos/bin/plumos-text-ui
dist/plumos-frontend/plumos/bin/plumos-controller-ui
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

Deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

Manual run:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

## Shell

```sh
./scripts/docker-build.sh shell
```

The repository root is mounted at `/workspace` inside the container.

## Deploy To A30

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

Run example:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

## Current Limits

- This Dockerfile is the starting point for the build/deploy loop.
- Final RetroArch and libretro core builds still need the A30 sysroot and
  library policy.
- Dynamically linked binaries must avoid depending on a glibc newer than the
  A30 stock glibc `2.23`, either through a dedicated sysroot or a bundled
  runtime.
- BusyBox is a useful first stage, but it still has limits for GNU/Linux-like
  `ps` and `top` compatibility. A more Debian-like workflow should add
  `procps-ng`, `coreutils`, `util-linux`, and similar tools under plumOS.
