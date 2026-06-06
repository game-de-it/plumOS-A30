# Docker Toolchain

plumOS development should generally build inside Docker, transfer artifacts to
the A30, validate on hardware, and collect logs.

## First Check

Build the Docker image.

```sh
./scripts/docker-build.sh image
```

Build the armhf smoke binary.

```sh
./scripts/docker-build.sh smoke
```

Outputs:

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

Build the plumOS userland.

```sh
./scripts/docker-build.sh userland
```

Outputs:

```text
dist/plumos-userland/plumos/bin/busybox
dist/plumos-userland/plumos/bin/plumos-env
dist/plumos-userland/plumos/share/doc/busybox/
```

Build the frontend prototype.

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

Build the runtime probe.

```sh
./scripts/docker-build.sh runtime-probe
```

Outputs:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

Build joystickd.

```sh
./scripts/docker-build.sh joystickd
```

Outputs:

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

Build the SDL2 probe.

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

## Deploy And Run On A30

With SSH running on the A30, deploy the smoke output.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

Run it on the device.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

Deploy the userland package to the SD card root.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-userland /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/plumos-env free -m'
```

Deploy the frontend prototype to the SD card root.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

Deploy the runtime probe to the SD card root.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

Deploy joystickd to the SD card root.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

Deploy and run the SDL2 probe together with
`plumos-joystickd --device-mode xbox`.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

Collect logs.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/collect-a30-logs.sh
```

## Policy

- Keep Dockerfiles, build scripts, patches, hashes, and recipes in git.
- Check the upstream latest stable release for libraries, RetroArch, cores, and
  emulators at build time, and record the selected version/tag/commit/build
  options in manifests.
- Keep `build/`, `dist/`, and `artifacts/` out of git as generated output.
- Handle dynamically linked binaries through an A30 sysroot or a bundled plumOS
  runtime.
- Add RetroArch and libretro cores to this build/deploy loop incrementally.
- Bundle a static BusyBox under plumOS to avoid quirks in the stock BusyBox.
- Add `procps-ng`, `coreutils`, `util-linux`, and similar tools later for a
  more Debian-like workflow.

See [plumOS design policy](plumos-design-policy.en.md) for more detail.
