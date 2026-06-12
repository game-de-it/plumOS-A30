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
dist/plumos-userland/plumos/gnu/bin/ip
dist/plumos-userland/plumos/gnu/bin/ss
dist/plumos-userland/plumos/gnu/bin/strace
dist/plumos-userland/plumos/gnu/libexec/
dist/plumos-userland/plumos/lib/
dist/plumos-userland/plumos/share/doc/busybox/
dist/plumos-userland/plumos/share/doc/gnu-userland/
```

BusyBox applet は vfat 向けに symlink ではなく wrapper script として生成します。
`iproute2` の `ip`/`ss` と `strace` は Debian armhf package 由来の実体を
`plumos/gnu/libexec` に置き、`plumos/gnu/bin` の wrapper から plumOS 同梱
dynamic loader と library path で起動します。

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
dist/plumos-frontend/plumos/bin/plumos-safe-hotkeyd
dist/plumos-frontend/plumos/bin/plumos-safe-shutdown
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
dist/plumos-runtime-probe/plumos/bin/plumos-shm-watch
dist/plumos-runtime-probe/plumos/bin/plumos-serial-joy-probe
dist/plumos-runtime-probe/plumos/bin/plumos-fb-restore
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

libretro core package を build します。core recipe は
`docker/plumos-toolchain/libretro-core-recipes.tsv` を正本にし、Onion が採用している
core は Onion 側の source 由来、実績 commit/build recipe を優先します。Onion に無い
plumOS 独自採用 core は upstream latest/HEAD 候補として残します。既定の
`PLUMOS_CORE_FILTER=plumos` は plumOS default の Class A/B 41 core を対象にし、
`PLUMOS_CORE_FILTER=onion` または `all` は Onion catalog 補完用 Class O も対象にします。
`PLUMOS_CORE_FILTER=quicknes` や `PLUMOS_CORE_FILTER=mednafen_vb` のように指定すると、
個別/クラス別 build もできます。選んだ commit、build flags、NEEDED は manifest に
残します。

```sh
./scripts/docker-build.sh libretro-cores
```

生成物は以下に出ます。

```text
dist/plumos-libretro-cores/plumos/retroarch/cores/*_libretro.so
dist/plumos-libretro-cores/plumos/retroarch/info/*_libretro.info
dist/plumos-libretro-cores/plumos/lib/
dist/plumos-libretro-cores/docs/manifest.txt
```

Onion catalog と plumOS recipe の差分は host 側で監査できます。

```sh
./scripts/inventory-onion-libretro-cores.sh
```

Onion prebuilt core の version/provenance と plumOS recipe の対応表は以下で再生成できます。

```sh
./scripts/inventory-libretro-core-versions.py > docs/libretro-core-version-inventory.tsv
```

別の recipe file を試す場合は、container 内 path として `CORE_RECIPES` を指定します。

```sh
CORE_RECIPES=/workspace/docker/plumos-toolchain/libretro-core-recipes.tsv \
PLUMOS_CORE_FILTER=onion \
./scripts/docker-build.sh libretro-cores
```

standalone emulator package を build します。現在は PPSSPP、ScummVM、EasyRPG Player、
DOSBox Staging、PCSX-ReARMed、Red Viper を A30 armv7 hard-float 向けに build し、選んだ
tag/commit、NEEDED、build log を manifest に残します。`PLUMOS_STANDALONE_FILTER=ppsspp`
のように指定すると個別 build もできます。`FAIL_ON_STANDALONE_ERROR=1` を指定すると、
いずれかの emulator build 失敗時に command 全体を失敗扱いにします。

```sh
./scripts/docker-build.sh standalone-emulators
```

生成物は以下に出ます。

```text
dist/plumos-standalone-emulators/plumos/emulators/<id>/bin/
dist/plumos-standalone-emulators/plumos/emulators/ppsspp/assets/
dist/plumos-standalone-emulators/plumos/bin/plumos-standalone-launch
dist/plumos-standalone-emulators/plumos/lib/
dist/plumos-standalone-emulators/docs/manifest.txt
dist/plumos-standalone-emulators/docs/build-logs/
```

## launch profile と生成物の照合

libretro core や standalone emulator を FE から選べるようにするには、
`package/frontend/plumos/config/frontend/systems.json` の `launch_profiles` に
`retroarch:<core_id>` または `standalone:<emulator_id>` を登録する必要があります。
逆に、生成物だけを deploy しても `systems.json` に無ければ Core Settings には出ません。

host 側では `scripts/audit-launch-profiles.py` で、`systems.json` と staging 済み生成物を
照合できます。

```sh
./scripts/audit-launch-profiles.py \
  --root dist/plumos-libretro-cores \
  --root dist/plumos-standalone-emulators
```

個別 core/emulator を追加するときは、対象 profile だけに絞ると判断しやすくなります。

```sh
PLUMOS_CORE_FILTER=quicknes \
TARGET_DIR=/workspace/dist/plumos-libretro-cores-quicknes \
./scripts/docker-build.sh libretro-cores

./scripts/audit-launch-profiles.py \
  --root dist/plumos-libretro-cores-quicknes \
  --profile retroarch:quicknes \
  --strict --fail-on-extra
```

`--strict` は登録済み profile の実体が無い場合に失敗します。`--fail-on-extra` は生成物が
あるのに `systems.json` から参照されていない場合に失敗します。全 core の bulk staging は
未検証/未採用候補も含むため、通常は warning として見て、対象を決めてから
`systems.json` に追加します。core package だけを確認するときは `--runtime retroarch`、
standalone emulator package だけを確認するときは `--runtime standalone` を指定します。
`enabled:false` の system は通常チェックから除外されます。

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

`deploy-a30.sh` は既存の plumOS mutable settings
(`plumos/config/frontend/settings.json`, `plumos/config/system/settings.json`,
`plumos/retroarch/config/retroarch-practical.cfg`,
`plumos/config/standalone/`, `plumos/state/standalone/ppsspp/` など) を
展開前に退避し、展開後に復元します。
初回導入でファイルが存在しない場合は package 側の default を配置します。設定を意図的に初期化したい場合だけ
`PLUMOS_DEPLOY_PROTECT_CONFIG=0` を指定します。

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

NES/GB core smoke は、SD カード root へ core package を展開してから実行します。
probe は framebuffer を RetroArch に渡すため `plumos-controller-ui-mali` を一時停止し、
終了後に再起動します。2026-06-07 時点で `fceumm` + NES と `gambatte` + GB は
`result=libretro_core_smoke_ok` になり、ユーザー目視でも両方のゲーム画面表示を確認済みです。
現在の `retroarch-minimal.cfg` は audio disabled なので、音声は full runtime build の
検証対象です。

2026-06-07 時点の bulk core build は 41本を stage 済みです。実機での画面/音/入力確認は
`fceumm`/`gambatte` 以外まだ未実施なので、deploy 後は system ごとの段階検証を行います。

```sh
./scripts/docker-build.sh libretro-cores
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-libretro-cores /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-libretro-cores.sh --duration 6
```

probe log は以下に残ります。

```text
/mnt/SDCARD/plumos/retroarch/logs/libretro-fceumm-last.log
/mnt/SDCARD/plumos/retroarch/logs/libretro-gambatte-last.log
```

log を回収します。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/collect-a30-logs.sh
```

## 方針

- Dockerfile、build script、patch、hash、recipe は git で管理する
- ライブラリ、RetroArch、standalone emulator は build 時点の upstream latest stable を確認し、
  選んだ version/tag/commit/build option を manifest に残す
- libretro core は Onion 採用 core catalog と実績 commit/build recipe を優先し、
  Onion に無い plumOS 独自 core は upstream latest/HEAD 候補として扱う。recipe file、
  選んだ commit、build option を manifest に残す
- `build/`, `dist/`, `artifacts/` は生成物として git に入れない
- 動的 link が必要なものは、A30 向け sysroot または plumOS 同梱 runtime で扱う
- RetroArch と libretro core は、この build/deploy loop に載せて段階的に増やす
- stock BusyBox の癖を避けるため、plumOS 側に静的 BusyBox を同梱する
- Debian に近い操作感は、次段階で `procps-ng`, `coreutils`, `util-linux` などを
  plumOS 側に追加して実現する

詳細は [plumOS 設計方針](plumos-design-policy.md) も参照してください。
