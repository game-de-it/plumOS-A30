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
dist/plumos-userland/plumos/gnu/bin/ip
dist/plumos-userland/plumos/gnu/bin/ss
dist/plumos-userland/plumos/gnu/bin/strace
dist/plumos-userland/plumos/gnu/libexec/
dist/plumos-userland/plumos/lib/
dist/plumos-userland/plumos/share/doc/busybox/
dist/plumos-userland/plumos/share/doc/gnu-userland/
```

BusyBox applets are installed as wrapper scripts instead of symlinks for vfat.
`iproute2` `ip`/`ss` and `strace` come from Debian armhf packages; the real
binaries live in `plumos/gnu/libexec`, and `plumos/gnu/bin` wrappers run them
through the plumOS-bundled dynamic loader and library path.

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
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali
dist/plumos-frontend/plumos/bin/plumos-controller-ui-mali.bin
dist/plumos-frontend/plumos/bin/plumos-safe-hotkeyd
dist/plumos-frontend/plumos/bin/plumos-safe-shutdown
dist/plumos-frontend/plumos/lib/
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
dist/plumos-runtime-probe/plumos/bin/plumos-shm-watch
dist/plumos-runtime-probe/plumos/bin/plumos-serial-joy-probe
dist/plumos-runtime-probe/plumos/bin/plumos-fb-restore
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

Build the upstream SDL2-compatible runtime. By default this fetches SDL3 3.4.10
and sdl2-compat 2.32.68, then records the tag, URL, SHA-256, and build options
in the manifest.

```sh
./scripts/docker-build.sh sdl2-runtime
```

Outputs:

```text
dist/plumos-sdl2-runtime/plumos/lib/
dist/plumos-sdl2-runtime/plumos/share/doc/plumos-sdl2-runtime/
```

Build the SDL2 probe. By default it bundles the runtime above.

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

The same probe binary is used for GameController checks and for enumerating SDL
video/render backends and attempting a software-renderer draw.

Build the RetroArch minimal display probe. This build does not handle
core/audio/input yet; it is a short device check for whether RetroArch itself
can show RGUI through the A30 Mali fbdev path.

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

Build the libretro core package. The current target builds 41 Class A/B cores
for the A30 armv7 hard-float environment and records selected commits, flags,
and NEEDED entries in the manifest. You can also filter the build with values
such as `PLUMOS_CORE_FILTER=vecx` or `PLUMOS_CORE_FILTER=class-a`.

```sh
./scripts/docker-build.sh libretro-cores
```

Outputs:

```text
dist/plumos-libretro-cores/plumos/retroarch/cores/*_libretro.so
dist/plumos-libretro-cores/plumos/retroarch/info/*_libretro.info
dist/plumos-libretro-cores/plumos/lib/
dist/plumos-libretro-cores/docs/manifest.txt
```

Build the standalone emulator package. The current target builds PPSSPP,
ScummVM, EasyRPG Player, DOSBox Staging, and PCSX-ReARMed for the A30 armv7
hard-float environment and records selected tags/commits, NEEDED entries, and
build logs in the manifest. You can filter the build with values such as
`PLUMOS_STANDALONE_FILTER=ppsspp`. Set `FAIL_ON_STANDALONE_ERROR=1` to make the
whole command fail when any emulator build fails.

```sh
./scripts/docker-build.sh standalone-emulators
```

Outputs:

```text
dist/plumos-standalone-emulators/plumos/emulators/<id>/bin/
dist/plumos-standalone-emulators/plumos/emulators/ppsspp/assets/
dist/plumos-standalone-emulators/plumos/bin/plumos-standalone-launch
dist/plumos-standalone-emulators/plumos/lib/
dist/plumos-standalone-emulators/docs/manifest.txt
dist/plumos-standalone-emulators/docs/build-logs/
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

The Mali EGL controller UI renderer is included in the same frontend package.
The wrapper uses the bundled dynamic loader/glibc, but it does not export
`LD_LIBRARY_PATH` to child processes, so scanner calls from inside the UI do not
break the stock `/bin/sh`.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --script down,a,b,q
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 2 --exercise 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock --no-scan --timeout 5 --exercise 2 --rotation auto
```

For plumOS-target tests, `--stop-mainui --stop-keymon --no-restart-stock` leaves
stock `/etc/main`, `MainUI.stock`, and `keymon` stopped. Because the A30
framebuffer is `480x640`, `--rotation auto` draws the landscape UI in the same
raw orientation as stock. Wi-Fi/SSH were confirmed to stay up through
`wpa_supplicant`/`udhcpc`/`dropbear`.

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

Build, deploy, and run the Mali EGL presenter probe.

```sh
./scripts/docker-build.sh mali-egl-probe
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

Deploy and run the SDL2 probe together with
`plumos-joystickd --device-mode xbox`.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

To check only the SDL2 render backends:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-render.sh --deploy --run-ms 100
```

Deploy the RetroArch minimal display probe to the SD card root and run it. By
default the script uses plumOS-target test conditions: it stops stock
`MainUI`/`keymon` and does not restart stock processes afterward. Use
`--rotation ccw` to show the RGUI horizontally on the physical A30 screen.

```sh
./scripts/docker-build.sh retroarch-minimal
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-minimal.sh --deploy --duration 10 --rotation ccw
```

The probe saves `/tmp/plumos-retroarch-minimal.log` to
`/mnt/SDCARD/plumos/retroarch/logs/minimal-last.log`.

For the NES/GB core smoke, deploy the core package to the SD card root and run
the probe. The script stops `plumos-controller-ui-mali` while RetroArch owns the
framebuffer, then restarts it afterward. As of 2026-06-07, `fceumm` + NES and
`gambatte` + GB both return `result=libretro_core_smoke_ok`, and both game
screens were visually confirmed on the A30. The current
`retroarch-minimal.cfg` disables audio, so sound remains part of the full
runtime validation.

As of 2026-06-07, the bulk core build stages 41 cores. Real-device screen,
audio, and input validation is still only done for `fceumm`/`gambatte`; the rest
must be checked per system after deployment.

```sh
./scripts/docker-build.sh libretro-cores
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-libretro-cores /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-libretro-cores.sh --duration 6
```

Probe logs are saved to:

```text
/mnt/SDCARD/plumos/retroarch/logs/libretro-fceumm-last.log
/mnt/SDCARD/plumos/retroarch/logs/libretro-gambatte-last.log
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
