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
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali.bin
dist/plumos-frontend/plumos/lib/
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

Mali EGL renderer device check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --script down,a,b,q
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 2 --exercise 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock --no-scan --timeout 5 --exercise 2 --rotation auto
```

For plumOS-target tests, use the last example to stop stock `/etc/main`,
`MainUI.stock`, and `keymon`. Because the A30 framebuffer is `480x640`,
`--rotation auto` draws the landscape UI in the same raw orientation as stock.

## Runtime Probe Build

```sh
./scripts/docker-build.sh runtime-probe
```

Outputs:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

Deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

Run:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

## joystickd Build

```sh
./scripts/docker-build.sh joystickd
```

Outputs:

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

Deploy:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
```

Read check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

## Mali EGL Probe Build

```sh
./scripts/docker-build.sh mali-egl-probe
```

Outputs:

```text
dist/plumos-mali-egl-probe/plumos/bin/plumos-mali-egl-probe
dist/plumos-mali-egl-probe/plumos/bin/plumos-mali-egl-probe.bin
dist/plumos-mali-egl-probe/plumos/lib/
dist/plumos-mali-egl-probe/plumos/share/doc/plumos-mali-egl-probe/
```

Device check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

## SDL2 Runtime Build

```sh
./scripts/docker-build.sh sdl2-runtime
```

Outputs:

```text
dist/plumos-sdl2-runtime/plumos/lib/
dist/plumos-sdl2-runtime/plumos/share/doc/plumos-sdl2-runtime/
```

By default this builds upstream SDL3 3.4.10 and sdl2-compat 2.32.68, recording
the tag, URL, SHA-256, and build options in the manifest.

## SDL2 Probe Build

```sh
./scripts/docker-build.sh sdl2-probe
```

Outputs:

```text
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe.bin
dist/plumos-sdl2-probe/plumos/lib/
dist/plumos-sdl2-probe/plumos/share/doc/plumos-sdl2-probe/
```

By default this bundles the `sdl2-runtime` output.

Device check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

SDL2 render backend check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-render.sh --deploy --run-ms 100
```

## RetroArch Minimal Display Probe Build

```sh
./scripts/docker-build.sh retroarch-minimal
```

Outputs:

```text
dist/plumos-retroarch-minimal/plumos/bin/plumos-retroarch-minimal
dist/plumos-retroarch-minimal/plumos/retroarch/bin/retroarch
dist/plumos-retroarch-minimal/plumos/retroarch/bin/retroarch.bin
dist/plumos-retroarch-minimal/plumos/retroarch/config/retroarch-minimal.cfg
dist/plumos-retroarch-minimal/plumos/lib/
dist/plumos-retroarch-minimal/docs/manifest.txt
```

This is the smallest current build for checking whether RetroArch itself can
display RGUI through the A30 Mali fbdev path. It is not the final
core/audio/input runtime yet.

Device check:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-minimal.sh --deploy --duration 10 --rotation ccw
```

`--rotation ccw` displays the RGUI horizontally on the physical A30 screen.

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
- For libraries, RetroArch, libretro cores, and standalone emulators, check the
  upstream latest stable release at build time and record the selected
  version/tag/commit/build options in manifests.
- The RetroArch minimal display probe is for A30 real-screen validation. The
  final RetroArch/libretro core runtime still needs the A30 sysroot and library
  policy.
- Dynamically linked binaries must avoid depending on a glibc newer than the
  A30 stock glibc `2.23`, either through a dedicated sysroot or a bundled
  runtime.
- BusyBox is a useful first stage, but it still has limits for GNU/Linux-like
  `ps` and `top` compatibility. A more Debian-like workflow should add
  `procps-ng`, `coreutils`, `util-linux`, and similar tools under plumOS.
