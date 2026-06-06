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
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
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
```

Collect logs.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/collect-a30-logs.sh
```

## Policy

- Keep Dockerfiles, build scripts, patches, hashes, and recipes in git.
- Keep `build/`, `dist/`, and `artifacts/` out of git as generated output.
- Handle dynamically linked binaries through an A30 sysroot or a bundled plumOS
  runtime.
- Add RetroArch and libretro cores to this build/deploy loop incrementally.
- Bundle a static BusyBox under plumOS to avoid quirks in the stock BusyBox.
- Add `procps-ng`, `coreutils`, `util-linux`, and similar tools later for a
  more Debian-like workflow.

See [plumOS design policy](plumos-design-policy.en.md) for more detail.
