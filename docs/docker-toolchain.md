# Docker toolchain

plumOS は、基本的に Docker 内で build して、生成物を A30 へ転送して実機確認する流れで
開発します。

## 最初の確認

Docker image を作ります。

```sh
./scripts/docker-build.sh image
```

armhf の smoke binary を build します。

```sh
./scripts/docker-build.sh smoke
```

生成物は以下に出ます。

```text
dist/docker-smoke/plumos-smoke-armhf
dist/docker-smoke/plumos-smoke-armhf.sha256
dist/docker-smoke/plumos-smoke-armhf.manifest.txt
```

plumOS 用 userland を build します。

```sh
./scripts/docker-build.sh userland
```

生成物は以下に出ます。

```text
dist/plumos-userland/plumos/bin/busybox
dist/plumos-userland/plumos/bin/plumos-env
dist/plumos-userland/plumos/share/doc/busybox/
```

frontend prototype を build します。

```sh
./scripts/docker-build.sh frontend
```

生成物は以下に出ます。

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

runtime probe を build します。

```sh
./scripts/docker-build.sh runtime-probe
```

生成物は以下に出ます。

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

joystickd を build します。

```sh
./scripts/docker-build.sh joystickd
```

生成物は以下に出ます。

```text
dist/plumos-joystickd/plumos/bin/plumos-joystickd
dist/plumos-joystickd/plumos/bin/plumos-joystick-reader
dist/plumos-joystickd/plumos/share/doc/plumos-joystickd/
```

upstream SDL2 互換 runtime を build します。既定では SDL3 3.4.10 と
sdl2-compat 2.32.68 を取得し、tag/URL/SHA-256/build option を manifest に残します。

```sh
./scripts/docker-build.sh sdl2-runtime
```

生成物は以下に出ます。

```text
dist/plumos-sdl2-runtime/plumos/lib/
dist/plumos-sdl2-runtime/plumos/share/doc/plumos-sdl2-runtime/
```

SDL2 probe を build します。既定では上記 runtime を同梱します。

```sh
./scripts/docker-build.sh sdl2-probe
```

生成物は以下に出ます。

```text
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe.bin
dist/plumos-sdl2-probe/plumos/lib/
dist/plumos-sdl2-probe/plumos/share/doc/plumos-sdl2-probe/
```

同じ probe binary は GameController 確認に加えて、SDL video/render backend の列挙と
software renderer の描画試行にも使います。

RetroArch minimal display probe を build します。これは core/audio/input をまだ扱わず、
RetroArch 本体の RGUI が A30 の Mali fbdev 経路で実画面表示できるかを見るための
短時間確認用 build です。

```sh
./scripts/docker-build.sh retroarch-minimal
```

生成物は以下に出ます。

```text
dist/plumos-retroarch-minimal/plumos/bin/plumos-retroarch-minimal
dist/plumos-retroarch-minimal/plumos/retroarch/bin/retroarch
dist/plumos-retroarch-minimal/plumos/retroarch/bin/retroarch.bin
dist/plumos-retroarch-minimal/plumos/retroarch/config/retroarch-minimal.cfg
dist/plumos-retroarch-minimal/plumos/lib/
dist/plumos-retroarch-minimal/docs/manifest.txt
```

## A30 へ転送して実行

A30 の SSH が起動している状態で転送します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/docker-smoke /mnt/SDCARD/plumos/smoke
```

実機上で実行します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh /mnt/SDCARD/plumos/smoke/plumos-smoke-armhf
```

userland package は SD カード root に展開します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-userland /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/plumos-env free -m'
```

frontend prototype も SD カード root に展開します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-frontend /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'PLUMOS_FRONTEND_MODE=manual /mnt/SDCARD/plumos/bin/plumos-frontend'
```

Mali EGL renderer 付き controller UI は、同じ frontend package に含まれます。
wrapper は bundled dynamic loader/glibc を使いますが、子プロセスへ
`LD_LIBRARY_PATH` は渡さず、UI 内の scanner 呼び出しが stock `/bin/sh` を壊さないように
しています。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --deploy --timeout 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --script down,a,b,q
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --no-scan --timeout 2 --exercise 3
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock --no-scan --timeout 5 --exercise 2 --rotation auto
```

plumOS としての本試験では `--stop-mainui --stop-keymon --no-restart-stock` で stock
`/etc/main`、`MainUI.stock`、`keymon` を止めたままにします。A30 の `/dev/fb0` は
`480x640` のため、`--rotation auto` で stock と同じ raw 向きに横画面 UI を描きます。
Wi-Fi/SSH は `wpa_supplicant`/`udhcpc`/`dropbear` により維持されることを確認済みです。

runtime probe も SD カード root に展開します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-input-compare --timeout-ms 100'
```

joystickd も SD カード root に展開します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-joystickd /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-joystickd --no-uinput --timeout-ms 1000 --print-every 20'
```

Mali EGL presenter probe は、SD カード root へ展開して実行します。

```sh
./scripts/docker-build.sh mali-egl-probe
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

SDL2 probe は、`plumos-joystickd --device-mode xbox` と一緒に SD カード root へ展開して
実行します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

SDL2 render backend だけを確認する場合:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-render.sh --deploy --run-ms 100
```

RetroArch minimal display probe は、SD カード root へ展開して実行します。既定では
plumOS としての試験条件に寄せ、stock `MainUI`/`keymon` を止め、終了後に stock 側を
戻しません。A30 の物理画面で横向き RGUI を見る場合は `--rotation ccw` を使います。

```sh
./scripts/docker-build.sh retroarch-minimal
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-retroarch-minimal.sh --deploy --duration 10 --rotation ccw
```

この probe は `/tmp/plumos-retroarch-minimal.log` を
`/mnt/SDCARD/plumos/retroarch/logs/minimal-last.log` に保存します。

log を回収します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/collect-a30-logs.sh
```

## 方針

- Dockerfile、build script、patch、hash、recipe は git で管理する
- ライブラリ、RetroArch、core、emulator は build 時点の upstream latest stable を確認し、
  選んだ version/tag/commit/build option を manifest に残す
- `build/`, `dist/`, `artifacts/` は生成物として git に入れない
- 動的 link が必要なものは、A30 向け sysroot または plumOS 同梱 runtime で扱う
- RetroArch と libretro core は、この build/deploy loop に載せて段階的に増やす
- stock BusyBox の癖を避けるため、plumOS 側に静的 BusyBox を同梱する
- Debian に近い操作感は、次段階で `procps-ng`, `coreutils`, `util-linux` などを
  plumOS 側に追加して実現する

詳細は [plumOS 設計方針](plumos-design-policy.md) も参照してください。
