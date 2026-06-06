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

## Deploy And Run On A30

With SSH running on the A30, deploy the smoke output.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

Run it on the device.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
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

See [plumOS design policy](plumos-design-policy.en.md) for more detail.
