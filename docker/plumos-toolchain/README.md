# plumOS Toolchain Docker

plumOS 用の A30 build environment です。まずは Docker 内で armhf binary を作り、
生成物を `dist/` に出すところまでを最小ゴールにしています。

## image build

```sh
./scripts/docker-build.sh image
```

default image tag は `plumos-a30-toolchain:dev` です。変更する場合:

```sh
PLUMOS_DOCKER_IMAGE=plumos-a30-toolchain:local ./scripts/docker-build.sh image
```

## smoke build

```sh
./scripts/docker-build.sh smoke
```

生成物:

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

この smoke binary は `arm-linux-gnueabihf-gcc` で静的 link します。A30 の stock glibc
`2.23` に直接依存しない小さな実機確認用 binary として使います。

## plumOS userland build

```sh
./scripts/docker-build.sh userland
```

生成物:

```text
dist/plumos-userland/plumos/bin/busybox
dist/plumos-userland/plumos/bin/plumos-env
dist/plumos-userland/plumos/share/doc/busybox/
```

BusyBox は公式 `1.38.0` tarball を SHA-256 検証してから build します。SD カードは
vfat で symlink を扱いにくいため、各 applet は symlink ではなく小さな wrapper script
として生成します。

転送:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-userland /mnt/SDCARD
```

実行:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/plumos-env free -m'
```

## frontend prototype build

```sh
./scripts/docker-build.sh frontend
```

生成物:

```text
dist/plumos-frontend/plumos/bin/plumos-frontend
dist/plumos-frontend/plumos/bin/plumos-library-scan
dist/plumos-frontend/plumos/bin/plumos-text-ui
dist/plumos-frontend/plumos/bin/plumos-controller-ui
dist/plumos-frontend/plumos/share/doc/plumos-frontend/
```

転送:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
```

手動実行:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

## runtime probe build

```sh
./scripts/docker-build.sh runtime-probe
```

生成物:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

転送:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

実行:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'

A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

## joystickd build

```sh
./scripts/docker-build.sh joystickd
```

生成物:

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

転送:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
```

読み取り確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

## SDL2 runtime build

```sh
./scripts/docker-build.sh sdl2-runtime
```

生成物:

```text
dist/plumos-sdl2-runtime/plumos/lib/
dist/plumos-sdl2-runtime/plumos/share/doc/plumos-sdl2-runtime/
```

既定では upstream SDL3 3.4.10 と sdl2-compat 2.32.68 を build し、
tag/URL/SHA-256/build option を manifest に記録します。

## SDL2 probe build

```sh
./scripts/docker-build.sh sdl2-probe
```

生成物:

```text
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe.bin
dist/plumos-sdl2-probe/plumos/lib/
dist/plumos-sdl2-probe/plumos/share/doc/plumos-sdl2-probe/
```

既定では `sdl2-runtime` の成果物を同梱します。

実機確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

SDL2 render backend 確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-render.sh --deploy --run-ms 100
```

## shell

```sh
./scripts/docker-build.sh shell
```

repository root が container 内の `/workspace` として mount されます。

## A30 へ転送

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

実行例:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

## 現時点の制約

- この Dockerfile は最初の build/deploy loop を作るための土台です。
- ライブラリ、RetroArch、libretro core、standalone emulator は build 時点の
  upstream latest stable を確認し、選んだ version/tag/commit/build option を manifest
  に残します。
- RetroArch と libretro core の最終 build には、A30 向け sysroot と library 方針を
  追加していきます。
- 動的 link する binary は A30 の glibc `2.23` より新しい glibc へ依存しないよう、
  別途 sysroot または同梱 runtime を使う必要があります。
- BusyBox は便利な第一段階ですが、GNU/Linux らしい `ps` や `top` の互換性には限界が
  あります。Debian に近い操作感は `procps-ng`, `coreutils`, `util-linux` などを
  plumOS 側へ追加して実現します。
