# Pyxel A30 画面出力検討

調査日: 2026-06-14

## 目的

Pyxel を plumOS の実用アプリ実行環境として扱えるかを判断するため、Pyxel が想定する
画面出力経路、現行 plumOS runtime での成立可否、A30 の Mali/GLES hardware 経路へ
乗せられるかを確認した。

## Pyxel が想定する出力経路

Pyxel 2.9.6 は Python package 内の `pyxel_binding.abi3.so` から Rust 実装を呼ぶ。
Linux では `pyxel/__init__.py` が先に `libSDL2-2.0.so.0` を `RTLD_GLOBAL` で読み、
失敗した場合だけ package 同梱 SDL2 を読む。

Pyxel core 側の通常表示は SDL2 + OpenGL/GLES 前提で、SDL software renderer ではない。

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)`
- `SDL_CreateWindow(..., SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)`
- `SDL_GL_CreateContext`
  - OpenGL 2.1 を試す
  - 失敗時に OpenGL ES 2.0 へ fallback
- `SDL_GL_GetProcAddress` から `glow::Context` を作る
- Pyxel の 16 色 index screen を GL texture へ upload
- palette texture と shader で画面へ描画
- `SDL_GL_SwapWindow` で present

つまり、plumOS 側が用意すべき最小条件は「SDL2 API から GLES2 context と swap できる
video backend」である。これは Pyxel にとって hardware 描画経路になり得る。
ただし Pyxel の draw API 自体は CPU side の Canvas に描くため、GPU が肩代わりするのは
主に texture upload、拡大、shader、present である。

## 現行 plumOS SDL2 での結果

通常の `/mnt/SDCARD/plumos/bin/python3` wrapper は library path を以下に固定する。

```text
/mnt/SDCARD/plumos/python/lib:/mnt/SDCARD/plumos/lib:/usr/lib:/lib
```

このため Pyxel は plumOS 側の `/mnt/SDCARD/plumos/lib/libSDL2-2.0.so.0` を先に読む。
この SDL2 互換層で見える driver は以下。

```text
video: offscreen, dummy, evdev
audio: disk, dummy
```

実機で `pyxel.init(..., headless=False)` を試すと、以下のいずれかになる。

- driver 自動選択: `No available video device`
- `SDL_VIDEODRIVER=dummy`: window 作成時に OpenGL 不可
- `SDL_VIDEODRIVER=offscreen`: window 作成時に OpenGL 不可
- `SDL_VIDEODRIVER=evdev`: window 作成時に OpenGL 不可

したがって、現行 plumOS SDL2 互換層のままでは Pyxel の想定する通常画面出力には
対応できない。

## stock SDL2 mali backend での確認

実験として、Python wrapper ではなく dynamic loader を直接呼び、stock 側 SDL2 を
plumOS SDL2 より先に解決させた。

```sh
/mnt/SDCARD/plumos/lib/ld-linux-armhf.so.3 \
  --library-path /mnt/SDCARD/plumos/python/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/plumos/lib:/usr/lib:/lib \
  --argv0 /mnt/SDCARD/plumos/bin/python3 \
  /mnt/SDCARD/plumos/python/bin/python3.14 pyxel_stock_sdl_probe.py
```

stock SDL2 の driver は以下。

```text
video: mali, offscreen
audio: alsa, dsp, disk, dummy
```

Pyxel 通常初期化後の実機ログでは以下を確認した。

```text
current_video_driver mali
/usr/lib/libMali.so mapped
current_mode 480x640
MALI_CreateWindow:0x20000001 done.
```

`artifacts/pyxel-demo/stock-sdl-captures/20260614-081834.visible.png` で、Pyxel の
通常 SDL2/GLES 描画が実画面に出ていることを確認した。

ただし raw framebuffer 上では 480x640 portrait に対して読める向きで描画されており、
plumOS FE/RetroArch の横持ち表示規約とは合わない。実用化するには、SDL backend か
presenter 側で 640x480 logical landscape を報告し、480x640 physical surface へ
回転 present する必要がある。

## 実用化案

### 1. stock SDL2 を Pyxel launcher だけで使う

最短で Pyxel の通常 HW 経路を使える。今回の probe で `mali` driver、`libMali.so`、
実画面描画は成立した。

課題:

- stock library を runtime dependency にするため、plumOS/stockOS 境界方針上の理由付けが必要
- Pyxel 2.9.6 の SDL2 binding は新しめの SDL2 API 前提だが、stock SDL2 は 2.26.1 のため
  ABI/API 差分リスクが残る
- 横持ち回転が合っていない
- 入力、音声、終了処理、音量 hotkey 連動は未検証

### 2. plumOS SDL2 互換層へ A30 mali/fbdev video backend を追加する

本命案。Pyxel からは通常通り `libSDL2-2.0.so.0` を読むだけにし、plumOS 側 SDL2 が
A30 の `/dev/fb0` + `/dev/mali` + EGL/GLES2 を提供する。

必要な機能:

- `SDL_VIDEODRIVER=mali` もしくは A30 用 driver 名
- `SDL_GetCurrentDisplayMode` は Pyxel 向けに logical landscape、少なくとも 640x480 を
  扱えること
- `SDL_CreateWindow(... SDL_WINDOW_OPENGL ...)`
- `SDL_GL_CreateContext` で GLES2 context 作成
- `SDL_GL_GetProcAddress`
- `SDL_GL_SwapWindow`
- physical 480x640 surface への回転 present
- `/dev/fb0` owner が FE と重複しない launcher cleanup

`docs/a30-stock-sdl-video.md` の clean-room `plumos-mali-egl-probe` では
`eglInitialize`、GLES2 context、`gl renderer="Mali-400 MP"`、swap が確認済みなので、
技術的には成立可能と見る。

利点:

- Pyxel だけでなく、SDL2 + GLES を期待する他の standalone アプリにも効く
- plumOS 側 runtime として管理でき、stock SDL2 依存を避けられる
- 回転、解像度、FE復帰、volume/input policy を一箇所に寄せられる

### 3. Pyxel core に A30 専用 platform backend を追加する

Pyxel の Rust `platform` 実装を fork し、SDL video を経由せず A30 EGL presenter を直接
持たせる案。

成立はし得るが、Pyxel だけの fork maintenance になり、SDL2/GLES app 全体への恩恵がない。
plumOS としては優先度を下げる。

### 4. headless Pyxel + `/dev/fb0` blit

前回の確認で表示は可能。ただし Python から全画面変換して書く方式は重く、Pyxel の
通常 input/audio/window/frame loop ともずれるため、実用案からは外す。

## 実装結果

2026-06-14 の first pass では、plumOS SDL2 互換 runtime の下層 SDL3 に A30 専用
`a30mali` video backend を追加した。これは stock SDL2 binary を読まず、`/dev/fb0`、
`/dev/mali`、rootfs の `/usr/lib/libEGL.so` / `/usr/lib/libGLESv2.so` から Mali EGL
surface と GLES2 context を作る。

Pyxel 側は引き続き A30 専用 offscreen FBO presenter を使う。Pyxel の通常 renderer が app
本来の logical FBO へ描画した後、A30 presenter が GLES2 shader で 480x640 physical
framebuffer へ回転 present する。回転、拡大縮小、letterbox は GPU 側の texture sampling
で行い、CPU で画素を回転コピーしない。

追加した成果物:

- `docker/plumos-toolchain/patches/sdl3-3.4.10-a30mali-video-driver.patch`
- `docker/plumos-toolchain/patches/pyxel-2.9.6-a30-fbo-present.patch`
- `docker/plumos-toolchain/scripts/build-sdl2-runtime.sh` の `a30mali` patch 適用
- `docker/plumos-toolchain/scripts/build-pyxel-a30.sh`
- `scripts/docker-build.sh pyxel-a30`

build command:

```sh
scripts/docker-build.sh pyxel-a30
```

出力先:

```text
dist/plumos-pyxel-a30/
```

A30 へ deploy すると以下が追加される。

```text
/mnt/SDCARD/plumos/experiments/pyxel-a30-site/
/mnt/SDCARD/plumos/bin/plumos-pyxel-a30
/mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch
/mnt/SDCARD/plumos/share/doc/plumos-pyxel-a30/manifest.txt
```

`plumos-pyxel-a30` は Pyxel 用 Python 実行入口で、以下を既定値にする。

```text
SDL_VIDEODRIVER=a30mali
SDL_AUDIODRIVER=alsa
SDL_OPENGL_LIBRARY=/usr/lib/libGLESv2.so
SDL_EGL_LIBRARY=/usr/lib/libEGL.so
PLUMOS_PYXEL_A30_PRESENT=1
PLUMOS_PYXEL_A30_ROTATION=cw
SDL_GAMECONTROLLERCONFIG=plumOS A30 Gamepad 用 mapping
```

現在の既定 `SDL_VIDEODRIVER` は `a30mali` である。古い検証で使った stock SDL2 `mali`
driver は参照用に残すが、通常の Pyxel launcher では `/mnt/SDCARD/miyoo/lib` を
library path に入れない。

`PLUMOS_PYXEL_A30_ROTATION` は `cw`、`ccw`、`none` を受け付ける。A30 の横持ち表示では
`cw` が正しい向きだった。

`plumos-pyxel-a30-launch` は FE/ユーザー向けの wrapper で、Pyxel 実行前後に以下を行う。

- `plumos-volume-control apply` / `persist-runtime`
- `plumos-joystickd --device-mode xbox --trigger-mode buttons --shoulder-layout user`
- `plumos-safe-hotkeyd --volume-only`
- 終了時の `joystickd` / `safe-hotkeyd` cleanup

Pyxel 用の `SDL_GAMECONTROLLERCONFIG` は、`plumos-joystickd` の virtual pad を以下のように
SDL2 GameController へ見せる。

```text
A/B/X/Y: b0/b1/b2/b3
L/R: b4/b5
L2/R2: b6/b7 -> lefttrigger/righttrigger
SELECT/START/FUNCTION: back/start/guide
D-pad: hat0
left stick: a0/a1
```

stock SDL2 の既定 GameController mapping だけに任せると、START/SELECT/FUNCTION や
L/R/L2/R2 がずれる。このため Pyxel launcher では mapping override を必須とする。

patched Pyxel は通常の pip site-packages ではなく、以下の分離 site に置く。

```text
/mnt/SDCARD/plumos/experiments/pyxel-a30-site/
```

`plumos-pyxel-a30` / `plumos-pyxel-a30-launch` はこの site を `PYTHONPATH` の先頭に入れる。
そのため、ユーザーが通常の `pip install --upgrade pyxel` を行っても、Pyxel app launcher 経由
では patched Pyxel が優先される。反対に、`/mnt/SDCARD/plumos/bin/python3 -m pyxel` を直接
使うと pip 側の通常 Pyxel を読む可能性があるため、A30 画面出力が必要な Pyxel app は
`plumos-pyxel-a30-launch` 経由に固定する。

実機確認では plumOS SDL runtime の `a30mali` を明示して、SDL2 API から GLES2 context と
swap が成立することを確認した。

```text
compiled_sdl=2.32.68 linked_sdl=2.32.68
video_driver index=0 name="a30mali"
sdl init=yes current_video_driver=a30mali
video_display index=0 name="1" bounds=0,0 480x640
gl vendor="ARM" renderer="Mali-400 MP"
gl swap=yes
```

Pyxel 実機確認では `plumos-pyxel-a30-launch /mnt/SDCARD/plumos/demos/pyxel_validation_probe.py`
で以下を確認した。

```text
env SDL_VIDEODRIVER=a30mali
env SDL_AUDIODRIVER=alsa
plumOS Pyxel A30 present enabled logical=640x480 physical=480x640 rotation=cw
summary events=0 frames=119 seconds=5.01
```

capture:

```text
artifacts/pyxel-demo/a30-present-captures/20260614-085325.visible.cw.png
artifacts/pyxel-demo/a30mali-captures/20260614-115752.visible.cw.png
artifacts/pyxel-demo/a30mali-lastemulator-captures/20260614-120249.visible.cw.png
```

この capture で Pyxel の 640x480 landscape 画面が正しい向きに表示されることを確認した。
さらに `/mnt/SDCARD/Roms/pyxel/LastEmulator.pyxapp` は app 本来の `720x480` logical size で
初期化されることを確認した。`.pyxapp` は Python script として直接渡さず、
`plumos-pyxel-a30-launch -m pyxel play /mnt/SDCARD/Roms/pyxel/LastEmulator.pyxapp`
で起動する。`LastEmulator.pyxapp` はタイトル表示まで時間がかかるため、画面 capture は
起動後 45 秒以上待ってから行う。

## 音声・入力・hotkey 検証

2026-06-14 に plumOS SDL `a30mali` 経路の Pyxel で、音声、`plumos-joystickd --device-mode xbox`
入力、終了/cleanup、音量 hotkey への影響を確認した。

使用した probe:

```text
/mnt/SDCARD/plumos/demos/pyxel_validation_probe.py
/mnt/SDCARD/plumos/demos/pyxel_guided_mapping_probe.py
```

確認結果:

- Pyxel process が `/dev/snd/pcmC0D0p`、`/dev/snd/controlC0`、`/dev/snd/timer` を開き、
  `SDL_AUDIODRIVER=alsa` で音声経路に乗る。
- Pyxel process が `/dev/fb0`、`/dev/mali`、`/dev/input/event4` を開き、画面、Mali、
  `plumOS A30 Gamepad` を同時に使う。
- `plumos-joystickd --device-mode xbox` は `plumOS A30 Gamepad` を `/dev/input/js0` /
  `/dev/input/event4` として作成する。
- `plumos-safe-hotkeyd --volume-only` は Pyxel 実行中に `/dev/input/event3` を読み、
  `volume_up` / `volume_down` を `plumos-volume-control runtime-up/down` として処理できる。
- wrapper 終了後、`plumos-joystickd`、`plumos-safe-hotkeyd`、Pyxel process の残留はない。
- `plumos-volume-control persist-runtime` で runtime volume は終了時に保存される。

guided mapping で確認した物理ボタンと Pyxel 側ラベル:

```text
physical A        -> Pyxel A
physical B        -> Pyxel B
physical X        -> Pyxel X
physical Y        -> Pyxel Y
physical D-pad    -> Pyxel UP/DOWN/LEFT/RIGHT
physical START    -> Pyxel START
physical SELECT   -> Pyxel BACK
physical FUNCTION -> Pyxel GUIDE
physical L        -> Pyxel L
physical R        -> Pyxel R
physical L2       -> Pyxel LTRIG
physical R2       -> Pyxel RTRIG
left stick        -> Pyxel LX/LY
```

重要な注意:

- `--shoulder-layout standard` では A30 物理 L/R と L2/R2 が Pyxel 側で逆に見える。
- Pyxel 用 wrapper では `--shoulder-layout user` を既定にする。
- Pyxel の analog key に `btnp()` / `btnr()` を使うと panic するため、axis は `btnv()` で読む。

主な検証ログ:

```text
/mnt/SDCARD/plumos/logs/pyxel-validation/20260614-002327-audio-fd
/mnt/SDCARD/plumos/logs/pyxel-validation/20260614-003909-guided-buttons
/mnt/SDCARD/plumos/logs/pyxel-validation/20260614-004558-shoulder-user
/mnt/SDCARD/plumos/logs/pyxel-validation/20260614-004849-launch-wrapper
/mnt/SDCARD/plumos/logs/pyxel-lastemulator/20260614-010902-fit
```

## 現時点の結論

- plumOS SDL2 互換 runtime に `a30mali` backend を追加し、stock SDL2 binary なしで
  SDL2/GLES app が `/dev/fb0` + Mali EGL へ乗れる状態になった。
- Pyxel の通常出力は GLES2 前提なので、A30 の Mali hardware 経路へ乗せられる。
- 今回の Pyxel 専用 FBO presenter により、CPU software rotation なしで 640x480 logical
  landscape から A30 physical framebuffer へ表示できることを確認した。
- Pyxel の音声、入力、volume hotkey、cleanup は plumOS SDL `a30mali` 経路でも成立する。
- first pass の `a30mali` は SDL window + GLES context + swap を優先した最小 backend。
  SDL_Renderer や汎用window管理を要求するappは、追加実装が必要になる可能性がある。
