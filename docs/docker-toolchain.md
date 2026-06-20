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
dist/plumos-userland/plumos/gnu/bin/rsync
dist/plumos-userland/plumos/gnu/bin/ss
dist/plumos-userland/plumos/gnu/bin/strace
dist/plumos-userland/plumos/gnu/libexec/
dist/plumos-userland/plumos/lib/
dist/plumos-userland/plumos/share/doc/busybox/
dist/plumos-userland/plumos/share/doc/gnu-userland/
```

BusyBox applet は vfat 向けに symlink ではなく wrapper script として生成します。
`iproute2` の `ip`/`ss`、`rsync`、`strace` は Debian armhf package 由来の実体を
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

Python 3 runtime を build します。既定では Python 3.14.6 を source から armv7
hard-float 向けに cross build し、`pip`/Pyxel が plumOS 同梱 glibc 2.36 経路で動く
ように loader wrapper と runtime library を同梱します。stockOS の glibc 2.23 では
PyPI の Pyxel manylinux wheel が要求する glibc 2.28+ を満たせないため、必ず
`/mnt/SDCARD/plumos/bin/python3` または `/mnt/SDCARD/plumos/bin/pip3` から起動します。

```sh
./scripts/docker-build.sh python-runtime
```

生成物は以下に出ます。

```text
dist/plumos-python-runtime/plumos/bin/python3
dist/plumos-python-runtime/plumos/bin/pip3
dist/plumos-python-runtime/plumos/bin/pyxel
dist/plumos-python-runtime/plumos/python/
dist/plumos-python-runtime/plumos/lib/
dist/plumos-python-runtime/plumos/share/doc/plumos-python-runtime/manifest.txt
```

`pip` の manylinux armv7l 判定は `sys.executable` の ELF header を見ますが、plumOS では
subprocess 安定化のため `sys.executable` を shell wrapper に向けます。その差を
`sitecustomize.py` で補正し、実機上の通常の `pip3 install pyxel` が
`manylinux_2_28_armv7l` wheel を選べるようにしています。

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

PicoArch package を build します。PicoArch 本体だけを build し、libretro core は
`dist/plumos-libretro-cores` ではなく実機の `/mnt/SDCARD/plumos/retroarch/cores` を共有します。
SDL1 は A30 stockOS 側の fbcon 実装を使うため、package には Docker 由来の
`libSDL-1.2.so.0` を入れません。plumOS glibc/libpng/zlib は picoarch 専用 libdir に stage します。
stock SDL1 の `640x480` video mode は実機 LCD 上で走査方向が崩れるため、plumOS patch では
SDL1 を入力/音声用途に残しつつ、ゲーム中は libretro core の RGB565 frame を A30 Mali/EGL
presenter へ直接渡します。menu などは従来の 640x480 software surface を presenter へ
渡します。presenter は `/usr/lib/libEGL.so` と `/usr/lib/libGLESv2.so` を
dlopen するため、picoarch binary 自体の NEEDED には EGL/GLES を増やしません。

既定の表示条件は `PLUMOS_PICOARCH_A30_MALI=1`、
`PLUMOS_PICOARCH_A30_ROTATION=ccw`、`PLUMOS_PICOARCH_A30_VSYNC=1`、
`PLUMOS_PICOARCH_A30_LINEAR=0` です。実機では `fceumm` + `いっき` で向きと左右反転の修正を
確認済みです。direct present は `Screen size` の `Native`、`Aspect`、`Full` と、
`Screen effect` の `None`、`Scanline`、`DMG`、`LCD` を GPU 側で処理します。
`DMG` は白寄せの液晶ブレンド、`LCD` は RGB サブピクセル風パターンとして shader で近似し、
従来 software scaler へ fallback しません。Core Settings では既存の
`retroarch:<core_id>` から対応する `picoarch:<core_id>` を検証用候補として自動追加しますが、
各 system の初期 default は `default_launch_profile` のままにします。実機で表示、操作、実速度が
成立しない core は自動候補から除外します。

BIOS/system directory は PicoArch 側 config の `bios_dir = /path` で指定できます。
config file は core と ROM directory ごとに
`/mnt/SDCARD/plumos/state/picoarch/.picoarch-<core>-<ROM_DIR>/picoarch.cfg` へ置かれます。
`bios_dir` が無い場合の既定値は `/mnt/SDCARD/Bios/<ROM_DIR>` です。plumOS 側の launch 既定値は
`/mnt/SDCARD/plumos/config/standalone/picoarch.env` の
`PLUMOS_PICOARCH_BIOS_DIR=/path` でも指定できますが、`picoarch.cfg` の `bios_dir` がある場合は
そちらが優先されます。BIOS pack が共有 root に置かれる core は launcher 側で
`/mnt/SDCARD/Bios` を既定値にします。対象 core の一覧は `docs/emulator-build-targets.md` を正とします。

```sh
./scripts/docker-build.sh picoarch
```

生成物は以下に出ます。

```text
dist/plumos-picoarch/plumos/bin/plumos-picoarch-launch
dist/plumos-picoarch/plumos/config/standalone/picoarch.env
dist/plumos-picoarch/plumos/emulators/picoarch/bin/picoarch
dist/plumos-picoarch/plumos/emulators/picoarch/lib/
dist/plumos-picoarch/plumos/share/doc/picoarch/manifest.txt
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
`retroarch:<core_id>`、`standalone:<emulator_id>` のいずれかを
登録する必要があります。
Core Settings は各 `retroarch:<core_id>` から対応する `picoarch:<core_id>` も自動的に候補へ
追加します。生成物だけを deploy しても、元になる `retroarch:<core_id>` または
`standalone:<emulator_id>` が `systems.json` に無ければ Core Settings には出ません。

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

Python runtime も SD カード root に展開します。初回は `ensurepip` で pip を展開し、
その後は通常の pip command として Pyxel を導入できます。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-python-runtime /mnt/SDCARD
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/python3 -m ensurepip --upgrade'
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/pip3 install --upgrade pyxel'
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh '/mnt/SDCARD/plumos/bin/python3 -c "import pyxel; print(pyxel.VERSION)"'
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
