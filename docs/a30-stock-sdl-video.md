# A30 stock SDL 画面出力経路

2026-06-07 に stockOS 側の SDL library と主要 binary を実機から取得し、
`strings`, ELF dynamic section, 実機上の SDL2 probe, 実行中 `MainUI.stock` の
`/proc` 情報で確認しました。stock binary は参照用です。plumOS で流用する場合は
事前に確認します。

## 結論

- stock SDL2 は `libSDL2-2.0.so.0.2600.1` で、実行時の
  `SDL_GetVersion()` は `2.26.1`。
- stock SDL2 の video driver は `mali` と `offscreen` の2つ。`dummy` video は
  build されていません。
- `SDL_VIDEODRIVER` 未指定では `mali` が選ばれ、480x640 display と
  `opengles2` renderer を作成できます。
- `mali` driver は SDL2 に追加された A30/Allwinner 向け custom backend と見られます。
  文字列上は `mali-fbdev`, `Mali EGL Video Driver`, `/dev/fb0`,
  `MALI_CreateWindow` を含みます。
- `libSDL2-2.0.so.0` 自体は `libEGL.so`, `libGLESv2.so`, `libMali.so` に
  dynamic section では直接リンクしていません。SDL2 側が EGL/GLES library を
  `dlopen` する構造です。
- rootfs の `/usr/lib/libEGL.so` と `/usr/lib/libGLESv2.so` は最終的に
  `/usr/lib/libMali.so` への symlink です。
- 実行中の stock `MainUI.stock` は stock SDL2 系 library と `/usr/lib/libMali.so`
  を map し、fd として `/dev/mali` と `/dev/fb0` を開いています。
- stock SDL1 は `fbcon` と `dummy` video を持ち、`/dev/fb0`, `FBIOPAN_DISPLAY`,
  `FBIOPUT_VSCREENINFO`, `SDL_VIDEO_FBCON_ROTATION` などの fbdev 経路を含みます。

## stock SDL2 probe

既存の `plumos-sdl2-probe.bin` を使い、SDL2 library だけ stock 側を先に解決する
一時 runtime で確認しました。probe 本体は新しい glibc を要求するため、
toolchain 側の armhf glibc 一式を `/tmp/plumos-stock-sdl-glibc` に一時展開して
実行しました。

代表出力:

```text
compiled_sdl=2.32.68 linked_sdl=2.26.1
video_drivers=2
video_driver index=0 name="mali"
video_driver index=1 name="offscreen"
current_mode 480x640
sdl init=yes current_video_driver=mali
video_display index=0 name="0" bounds=0,0 480x640
render_driver index=0 name="opengles2"
render_driver index=1 name="opengles"
render_driver index=2 name="software"
MALI_CreateWindow:0x20000001 done.
render info name="opengles2" flags=0xa max_texture=4096x4096
render present=yes readpixels=yes
```

`SDL_VIDEODRIVER=dummy` は以下の通り失敗します。

```text
sdl init=no error="dummy not available"
```

`SDL_VIDEODRIVER=offscreen` は software renderer で成功しますが、実画面出力では
ありません。

## build の手がかり

stock SDL2 の文字列には以下の path が残っています。

```text
/home/alfchen/ProjectData/miyoo_allwinner/a33_tina3.5.1/out/r16-parrot/compile_dir/target/libsdl2/src/render/opengles2/SDL_render_gles2.c
```

このため、stock SDL2 は Miyoo/Allwinner Tina 3.5.1 系の build tree で、
upstream SDL2 2.26.1 に A30 向け `mali` video backend を追加したものと推定します。
dynamic section の依存は以下のように一般的な libc 系のみで、EGL/GLES は動的ロードです。

```text
NEEDED libm.so.6
NEEDED libdl.so.2
NEEDED libpthread.so.0
NEEDED librt.so.1
NEEDED libgcc_s.so.1
NEEDED libc.so.6
SONAME libSDL2-2.0.so.0
```

rootfs 側の GPU library は以下の構造です。

```text
/usr/lib/libEGL.so -> libEGL.so.1 -> libEGL.so.1.4 -> libMali.so
/usr/lib/libGLESv1_CM.so -> ... -> libMali.so
/usr/lib/libGLESv2.so -> libGLESv2.so.2 -> libGLESv2.so.2.0 -> libMali.so
/usr/lib/libMali.so
/usr/lib/libUMP.so.3.0.0
```

`libMali.so` には `__egl_platform_*_fbdev`, `/dev/fb0`, `/dev/mali`,
`MALI_FBDEV` などの文字列があり、fbdev EGL platform を持っています。

## stock app 別の使い方

`MainUI.stock`:

- `libSDL2-2.0.so.0`, `libSDL2_ttf`, `libSDL2_image`, `libSDL2_mixer` にリンク。
- 文字列上は `SDL_CreateWindow`, `SDL_CreateRenderer`, `SDL_RenderPresent` を使用。
- 実行中は `/mnt/SDCARD/miyoo/lib/libSDL2-2.0.so.0` と `/usr/lib/libMali.so` を map。
- fd として `/dev/mali` と `/dev/fb0` を開く。

`sdlloading`:

- SDL1 を使い、`SDL_SetVideoMode`, `SDL_Flip` と `/dev/fb0` 文字列を持つ。
- loading 表示は SDL1 fbcon 経路の可能性が高いです。

`RetroArch`:

- `retroarch.cfg` は `video_driver = "gl"`, `input_driver = "sdl"`,
  `input_joypad_driver = "sdl"`。
- binary は `libSDL-1.2.so.0` と `libMali.so` にリンクし、`egl*`, `SDL_GL*`,
  `sdl_dingux`, `sunxi`, `/dev/fb0` 文字列を持ちます。
- 入力は SDL1 ですが、画面は単純な SDL1 fbcon だけではなく GL/EGL/Mali 経路も
  使う構成です。

`PPSSPPSDL`:

- `libSDL2-2.0.so.0`, `libMali.so`, `libEGL.so` にリンク。
- SDL2 の window/input と、PPSSPP 側の GLES/EGL renderer を組み合わせていると
  見られます。
- `launch.sh` は `miyoo282_xpad_inputd` を起動してから `PPSSPPSDL` を起動します。

## plumOS への示唆

upstream SDL3+sdl2-compat に stock SDL2 の `mali` driver はありません。このため、
plumOS の実画面 SDL 出力は次のいずれかで設計する必要があります。

- stock SDL2 の挙動を参考に、clean-room で fbdev + Mali EGL の presenter を作る。
- SDL3/SDL2 互換 runtime に A30 向け custom video backend を追加する。
- frontend は direct framebuffer presenter で始め、RetroArch/standalone emulator は
  各 upstream の EGL/GLES backend を A30 の `/usr/lib/libMali.so` に合わせて検証する。

stock binary/source を plumOS runtime に流用する場合は、方針メモ通り事前確認が必要です。
