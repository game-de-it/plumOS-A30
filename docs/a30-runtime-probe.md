# A30 runtime probe

`plumos-runtime-probe` は、A30 の frontend/runtime 置き換え前に、最小限の
video/input/audio interface を確認するための静的リンク診断 binary です。

`plumos-runtime-probe` は stock SDL library にはリンクせず、まず Linux interface を
直接確認します。SDL2 の動的 link/runtime 方針は、別の `plumos-sdl2-probe` で
検証します。

## build

```sh
./scripts/docker-build.sh runtime-probe
```

生成物:

```text
dist/plumos-runtime-probe/plumos/bin/plumos-runtime-probe
dist/plumos-runtime-probe/plumos/bin/plumos-input-compare
dist/plumos-runtime-probe/plumos/bin/plumos-shm-watch
dist/plumos-runtime-probe/plumos/bin/plumos-serial-joy-probe
dist/plumos-runtime-probe/plumos/share/doc/plumos-runtime-probe/
```

SDL2 linked/GameController probe:

```sh
./scripts/docker-build.sh sdl2-runtime
./scripts/docker-build.sh sdl2-probe
```

生成物:

```text
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe
dist/plumos-sdl2-probe/plumos/bin/plumos-sdl2-probe.bin
dist/plumos-sdl2-probe/plumos/lib/
dist/plumos-sdl2-probe/plumos/share/doc/plumos-sdl2-probe/
```

`plumos-sdl2-probe` は upstream SDL3 3.4.10 と sdl2-compat 2.32.68、その実行に
必要な dynamic loader / shared library を `/mnt/SDCARD/plumos/lib` へ同梱して実行します。
A30 の SD カードは symlink を作れないため、soname は symlink ではなく通常ファイルとして
複製します。

Mali EGL presenter probe:

```sh
./scripts/docker-build.sh mali-egl-probe
```

生成物:

```text
dist/plumos-mali-egl-probe/plumos/bin/plumos-mali-egl-probe
dist/plumos-mali-egl-probe/plumos/bin/plumos-mali-egl-probe.bin
dist/plumos-mali-egl-probe/plumos/lib/
dist/plumos-mali-egl-probe/plumos/share/doc/plumos-mali-egl-probe/
```

`plumos-mali-egl-probe` は stock SDL にはリンクせず、A30 rootfs の
`/usr/lib/libEGL.so` と `/usr/lib/libGLESv2.so` を `dlopen` します。probe 本体は
bundled dynamic loader/shared libraries で起動します。

## deploy/run

```sh
A30_TARGET=root@192.168.10.165 ./scripts/deploy-a30.sh dist/plumos-runtime-probe /mnt/SDCARD
```

安全な読み取り寄りの確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --input-ms 100 --no-audio'
```

短時間の framebuffer draw/restore と audio 状態確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-runtime-probe --draw-ms 80 --input-ms 100 --audio-ms 80 --allow-busy-audio'
```

`--allow-busy-audio` は、stock MainUI が PCM device を掴んでいる状態を失敗ではなく
観測結果として扱うための option です。

stock `keymon`/`MainUI` の SysV shared memory を read-only で監視:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-shm-watch --timeout-ms 10000 --interval-ms 100 --max-bytes 128'
```

`plumos-shm-watch` は、左スティック calibration のように kernel input event に出ない
経路を調べるための補助 binary です。shared memory へ書き込みは行いません。

serial joystick raw data を確認:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  '/mnt/SDCARD/plumos/bin/plumos-serial-joy-probe --timeout-ms 5000 --stats-only'
```

spruceOS では A30 joystick reader が `/dev/ttyS2` を使う実装になっています。一方、
2026-06-06 の stock A30 実機では `/dev/ttyS0` から joystick frame を確認しました。
`/dev/ttyS2` node は初期状態で存在せず、`/proc/tty/drivers` では `ttyS` minor 0-4 が
利用可能として表示されますが、一時 node `c 250 2` は `ENXIO` で open できませんでした。

```sh
A30_TARGET=root@192.168.10.165 ./scripts/run-a30.sh \
  'rm -f /tmp/plumos-ttyS2; mknod /tmp/plumos-ttyS2 c 250 2; /mnt/SDCARD/plumos/bin/plumos-serial-joy-probe --port /tmp/plumos-ttyS2 --timeout-ms 10000 --stats-only; rm -f /tmp/plumos-ttyS2'
```

`plumos-serial-joy-probe` は 9600/8N1 の raw serial と、spruceOS で使われている
可能性が高い `ff axisYL axisXL axisYR axisXR fe` 形式の6バイト frame を表示します。
`--stats-only` では frame ごとの出力を抑制し、各 axis の min/max/avg だけを表示します。

SDL2 linked/GameController probe:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-gamepad.sh --deploy --run-ms 5000
```

この script は `plumos-joystickd --device-mode xbox` を短時間起動し、
`plumOS A30 Gamepad` が SDL2 の Joystick/GameController として見えるか確認します。
既定では `SDL_JOYSTICK_DEVICE` を指定せず、SDL2 の通常の自動検出を使います。
`--force-js-device` を付けると、検出した `/dev/input/js*` を
`SDL_JOYSTICK_DEVICE` に渡す比較確認ができます。

SDL2 render backend probe:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-sdl2-render.sh --deploy --run-ms 100
```

この script は `SDL_VIDEODRIVER` なしの auto、`dummy`, `offscreen`, `evdev`,
`kmsdrm` を順に試し、window 作成、renderer 作成、`SDL_RenderPresent`、
`SDL_RenderReadPixels` の成否を記録します。

Mali EGL presenter probe:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 300 --frames 20
```

この script は `/dev/fb0`, `/dev/mali`, `/usr/lib/libEGL.so`,
`/usr/lib/libGLESv2.so` を確認し、`eglCreateWindowSurface`,
`eglMakeCurrent`, `eglSwapBuffers`, `glReadPixels` の成否を記録します。

## options

- `--fb PATH`: framebuffer path。既定値は `/dev/fb0`
- `--input PATH`: input event path。既定値は `gpio-keys-polled` の自動検出
- `--dsp PATH`: OSS audio path。既定値は `/dev/dsp`
- `--draw-ms MS`: 小さい framebuffer patch を描いて復元する時間
- `--input-ms MS`: input event を poll する時間
- `--audio-ms MS`: OSS へ短い test tone を書く時間
- `--allow-busy-audio`: audio device の `EBUSY` を成功扱いにする
- `--no-video`, `--no-input`, `--no-audio`: 個別 probe を省略

環境変数 `PLUMOS_FB`, `PLUMOS_INPUT_EVENT`, `PLUMOS_DSP` でも path を上書きできます。

## 2026-06-06 実機結果

read-only 寄りの確認:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32 line=1920
video draw=skipped
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio skipped
result=ok
```

draw/audio 状態確認:

```text
plumOS runtime probe
video path=/dev/fb0 open=yes xres=480 yres=640 virtual=480x1280 offset=0,0 bpp=32 line=1920
video draw=ok pixels=64x64 ms=80
input path=/dev/input/event3 open=yes timeout_ms=100 events=0 key_events=0
audio path=/dev/dsp open=busy allowed=yes errno=16 Device or resource busy
result=ok
```

`offset` は stock MainUI の framebuffer/pan 状態により `0,0` または `0,640` として
観測されることがあります。

追加確認では、stock `MainUI` が `/dev/snd/pcmC0D0p` を保持していました。そのため、
stock frontend を動かしたままでは `/dev/dsp` への test tone 書き込みはできません。

SDL2 linked/GameController probe:

```text
plumOS SDL2 probe
compiled_sdl=2.32.68 linked_sdl=2.32.68 timeout_ms=3000 no_video=no graphics_only=no render_test=no window_visible=no
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_JOYSTICK_DEVICE=-
video_drivers=3
video_driver index=0 name="offscreen"
video_driver index=1 name="dummy"
video_driver index=2 name="evdev"
sdl init=yes current_video_driver=dummy current_audio_driver=-
render_drivers=1
render_driver index=0 name="software" flags=0xd
window create=yes visible=no size=64x64 error=""
joysticks=1
device index=0 name="Xbox 360 Controller" guid=0300e4fb5e0400008e0200005e040000 is_controller=yes
controller open index=0 yes
controller info index=0 name="Xbox 360 Controller" attached=yes axes=6 buttons=11 hats=1
summary joysticks=1 controllers_open=1 joysticks_open=0 controller_events=1 joystick_events=1
result=sdl2_gamecontroller_visible
```

`SDL_VIDEODRIVER=dummy` で window 作成まで確認しました。実 framebuffer/render backend は
下の SDL2 render backend probe で別途確認しています。

`--force-js-device` で `SDL_JOYSTICK_DEVICE=/dev/input/js0` を渡すと、
SDL2 は同じ virtual pad を `plumOS A30 Gamepad` と `Xbox 360 Controller` の2 entry として
列挙しました。通常運用に近い条件では、環境変数なしの自動検出を優先します。

SDL2 render backend probe:

```text
== device inventory ==
/dev/fb0 ... /dev/fb7
/dev/disp
/dev/ion
/dev/mali
== proc fb ==
0
1
2
3
4
5
6
7
== case: auto ==
sdl init=no error="No available video device"
case_rc=1
== case: dummy ==
sdl init=yes current_video_driver=dummy current_audio_driver=-
render_driver index=0 name="software" flags=0xd
window create=yes visible=yes size=64x64 error=""
render create=default yes
render info name="software" flags=0x9 max_texture=0x0 formats=3
render present=yes readpixels=yes pixel_argb8888=0xff102040 error=""
case_rc=0
== case: offscreen ==
sdl init=yes current_video_driver=offscreen current_audio_driver=-
render create=default yes
render present=yes readpixels=yes pixel_argb8888=0xff102040 error=""
case_rc=0
== case: evdev ==
sdl init=yes current_video_driver=evdev current_audio_driver=-
render create=default yes
render present=yes readpixels=yes pixel_argb8888=0xff102040 error=""
case_rc=0
== case: kmsdrm ==
sdl init=no error="kmsdrm not available"
case_rc=1
== result ==
result=sdl2_renderer_only_dummy_offscreen_or_evdev
```

A30 には `/dev/fb*`, `/dev/mali`, `/dev/disp` はありますが `/dev/dri` はありません。
今回の upstream SDL3+sdl2-compat runtime では `offscreen`, `dummy`, `evdev` の
software renderer だけが使え、実画面に出る framebuffer/render backend は確認できません。
`evdev` は SDL3 の dummy video driver with evdev input で、実表示 backend ではありません。

Mali EGL presenter probe:

```text
plumOS Mali EGL probe
fb=/dev/fb0 egl_lib=/usr/lib/libEGL.so gles_lib=/usr/lib/libGLESv2.so window_mode=auto run_ms=300 frames=20
fb var=yes xres=480 yres=640 virtual=480x1280 offset=0,0 bpp=32 visual=2
egl initialize=yes version=1.4
egl version="1.4 Linux-r8p1-00rel0"
egl surface mode=null create=yes native=(nil) surface=0x20000001
egl surface_size=480x640 mode_used=null
egl context es2=yes context=0x40000001
egl make_current=yes
gl renderer="Mali-400 MP"
gl version="OpenGL ES 2.0 "mali450-r5p1-01rel0-lollipop-252-gcc9bf62""
draw frames=19 swap_ok=yes elapsed_ms=314
gl readpixels rgba=381f96ff
result=mali_egl_present_ok
```

これで、stock SDL にリンクしない clean-room の fbdev + Mali EGL presenter が
A30 上で実画面へ swap できることを確認しました。`NULL` native window と
`uint16_t width,height` の `fbdev_window` は成功し、`uint32_t width,height` は
`EGL_BAD_NATIVE_WINDOW` でした。

## 判断

- video: `/dev/fb0` は 480x640、32bpp、line length 1920 として取得でき、短時間の
  draw/restore も成功
- input: `gpio-keys-polled` は `/dev/input/event3` として open/poll 可能
- shared memory: `plumos-shm-watch` で stock `keymon`/`MainUI` の SysV shm を read-only
  attach できる
- serial joystick: `ttyS` driver は minor 0-4 を持つが、初期 `/dev` には
  `/dev/ttyS0` と `/dev/ttyS1` のみ存在する。`/dev/ttyS0` は 9600/8N1 で
  joystick frame を出力する
- audio: OSS `/dev/dsp` は存在するが、stock MainUI が PCM を保持している間は busy
- SDL2: plumOS 同梱 upstream SDL3 3.4.10 + sdl2-compat 2.32.68 と bundled
  dynamic loader/shared libraries で linked probe が起動し、
  `plumos-joystickd --device-mode xbox` の composite virtual pad を GameController として
  自動認識した
- SDL2 render: `dummy`/`offscreen`/`evdev` の software renderer は作成できるが、
  実画面に出る upstream SDL backend は無い。A30 は fbdev + Mali/disp 構成で
  `/dev/dri` が無く、`kmsdrm` は利用不可
- Mali EGL: stock SDL にリンクしない `plumos-mali-egl-probe` で
  `/usr/lib/libEGL.so`/`libGLESv2.so` を `dlopen` し、fbdev EGL surface、
  GLES2 context、`eglSwapBuffers`、`glReadPixels` まで成功した
- frontend Mali: 推奨案として frontend 直結の `plumos-controller-ui-mali` を追加し、
  TOP 表示、full scan、`down,a,b,q` による ROM list 遷移/復帰を A30 上で確認した

stock `keymon` と直接 input event の比較は [A30 input policy](a30-input-policy.md) に
分離しました。次は、`plumos-controller-ui-mali` の実画面視認/長時間操作確認と、
RetroArch SDL2/evdev build に進みます。SDL3/sdl2-compat custom video backend 化は
frontend presenter の挙動が固まってから再判断します。
