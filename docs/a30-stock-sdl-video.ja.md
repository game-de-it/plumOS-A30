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
- stock SDL1 の汎用 `fbcon` へ 640x480/16bpp をそのまま出すと、
  `/dev/fb0` capture は正常でも物理 LCD の走査が崩れます。StockOS の SDL1 利用
  binary は、純粋な SDL1 surface 表示だけではなく、fbdev 直接操作や GL/EGL 併用で
  A30 の 480x640 raw panel に合わせていると見ます。

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

外部参考として、[weimingtom/miyoo_a30_playground](https://github.com/weimingtom/miyoo_a30_playground)
も A30 SDL 調査に有用です。README には `--enable-video-a30`、
`libEGL.so`/`libGLESv2.so`、`src/video/a30/SDL_a30_video.c` の記述があります。
同 repo の `sdltest/sdl-main.tar.gz` には SDL1 A30 backend が含まれ、
`eglGetDisplay(EGL_DEFAULT_DISPLAY)` と `eglCreateWindowSurface(..., 0, ...)` を
使う構成を確認できます。これは下記の clean-room probe で `NULL` native window が
成功した結果と一致します。

## stock app 別の使い方

`MainUI.stock`:

- `libSDL2-2.0.so.0`, `libSDL2_ttf`, `libSDL2_image`, `libSDL2_mixer` にリンク。
- 文字列上は `SDL_CreateWindow`, `SDL_CreateRenderer`, `SDL_RenderPresent` を使用。
- 実行中は `/mnt/SDCARD/miyoo/lib/libSDL2-2.0.so.0` と `/usr/lib/libMali.so` を map。
- fd として `/dev/mali` と `/dev/fb0` を開く。

`sdlloading`:

- SDL1 を使い、`SDL_SetVideoMode`, `SDL_Flip` と `/dev/fb0` 文字列を持つ。
- `lcd_fb_init`, `sdl_flip`, `make_zoom_rotate_table90`, `orig: %dx%d -> %dx%d`,
  `set : %dx%d -> %dx%d` などの文字列を持つ。
- `./testm.bmp`, `./testr.bmp` を SDL surface として読み、90度回転/拡大縮小 table を
  作ってから `/dev/fb0` へ直接 mmap/pan する loading 専用処理に見えます。
- したがって、`sdlloading` が stock SDL1 を使うことは確認できますが、
  「汎用 SDL1 fbcon に 640x480 を渡せば A30 LCD で正しく見える」という証拠には
  なりません。

`RetroArch`:

- `retroarch.cfg` は `video_driver = "gl"`, `input_driver = "sdl"`,
  `input_joypad_driver = "sdl"`。
- binary は `libSDL-1.2.so.0` と `libMali.so` にリンクし、`egl*`, `SDL_GL*`,
  `sdl_dingux`, `sunxi`, `/dev/fb0` 文字列を持ちます。
- 入力は SDL1 ですが、画面は単純な SDL1 fbcon だけではなく GL/EGL/Mali 経路も
  使う構成です。
- stock launch script は主に `HOME=/mnt/SDCARD/RetroArch ra32.miyoo -L core rom`
  形式で、`SDL_VIDEO_FBCON_ROTATION` や `SDL_FBDEV` は明示していません。
- 一部 core config には `video_driver = "sdl"` が残っていますが、主要 global config は
  `video_driver = "gl"` です。RetroArch binary 自体にも EGL/OpenGLES 経路が含まれます。

`pcsx`, `sms.elf`:

- `libSDL-1.2.so.0` にリンクし、`SDL_SetVideoMode`, `SDL_UpperBlit`,
  `SDL_Flip` などを持ちます。
- どちらも `FBIOPAN_DISPLAY` 失敗時の文字列や `cat /dev/zero > /dev/fb0` を持ち、
  SDL1 surface だけで完結せず fbdev を直接扱う設計に見えます。

`PPSSPPSDL`:

- `libSDL2-2.0.so.0`, `libMali.so`, `libEGL.so` にリンク。
- SDL2 の window/input と、PPSSPP 側の GLES/EGL renderer を組み合わせていると
  見られます。
- `launch.sh` は `miyoo282_xpad_inputd` を起動してから `PPSSPPSDL` を起動します。

## plumOS への示唆

upstream SDL3+sdl2-compat に stock SDL2 の `mali` driver はありません。このため、
まず stock SDL にリンクしない `plumos-mali-egl-probe` を実装して、fbdev + Mali EGL
の最小 presenter を確認しました。

```sh
./scripts/docker-build.sh mali-egl-probe
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-mali-egl.sh --deploy --run-ms 500 --frames 30
```

実機結果:

```text
egl initialize=yes version=1.4
egl version="1.4 Linux-r8p1-00rel0"
egl surface mode=null create=yes native=(nil) surface=0x20000001
egl surface_size=480x640 mode_used=null
egl context es2=yes context=0x40000001
egl make_current=yes
gl renderer="Mali-400 MP"
draw frames=19 swap_ok=yes
gl readpixels rgba=381f96ff
result=mali_egl_present_ok
```

`NULL` native window と `uint16_t width,height` の `fbdev_window` はどちらも成功し、
`uint32_t width,height` は `EGL_BAD_NATIVE_WINDOW` でした。surface handle は
stock SDL2 probe と同じ `0x20000001` です。

今後の実画面出力は次のいずれかで設計します。

- stock SDL2 の挙動を参考に、clean-room で fbdev + Mali EGL の presenter を作る。
- SDL3/SDL2 互換 runtime に A30 向け custom video backend を追加する。
- frontend は direct framebuffer presenter で始め、RetroArch/standalone emulator は
  各 upstream の EGL/GLES backend を A30 の `/usr/lib/libMali.so` に合わせて検証する。

stock binary/source を plumOS runtime に流用する場合は、方針メモ通り事前確認が必要です。

## stock SDL1 fbcon probe

2026-06-15 に、stock `libSDL-1.2.so.0` を使う最小描画 probe
`plumos-sdl1-fbcon-probe` を作りました。package には Docker 由来の SDL1 を入れず、
runtime では `/mnt/SDCARD/miyoo/lib/libSDL-1.2.so.0` を先に解決します。

640x480/16bpp:

```text
SDL_GetVideoMode 640x480 16 -> 640x480 16
before fb xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32
Using VESA timings for 640x480
after_set_video fb xres=640 yres=480 virtual=640x480 offset=0,0 bpp=16
```

この状態の `/dev/fb0` capture は probe の絵として正常でしたが、実機 LCD では PicoArch と
同じように崩れて見えることを目視確認しました。A30 の物理 panel は 480x640 raw で、
SDL1 fbcon が VESA 640x480 mode set へ進むと scanout と合わないと判断します。

480x640/32bpp:

```text
SDL_GetVideoMode 480x640 32 -> 480x640 32
before fb xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32
after_set_video fb xres=480 yres=640 virtual=480x1280 offset=0,640 bpp=32
surface w=480 h=640 pitch=1920 bytespp=4
frames=603 elapsed_ms=20002 result=ok
```

こちらは VESA 640x480 path へ入らず native panel 相当の mode を維持します。PicoArch を
stock SDL1 で続ける場合の検証対象は、640x480 surface ではなく 480x640/32bpp surface へ
NES 画面を CPU または NEON で回転/scale して書く経路です。ただし、最終的には SDL1 fbcon
ではなく、既に plumOS で確認済みの fbdev + Mali EGL presenter へ接続する方が将来性が高いです。

## plumOS `a30mali` backend first pass

2026-06-14 に、SDL3+sdl2-compat runtime へ clean-room の A30 用 video backend
`a30mali` を追加しました。stock SDL2 binary は使わず、SDL3 の EGL helper から
`EGL_DEFAULT_DISPLAY`、rootfs の `/usr/lib/libEGL.so` / `/usr/lib/libGLESv2.so`、
`/dev/fb0`、`/dev/mali` を使います。

同日の後続実装で `PLUMOS_A30MALI_ROTATION=cw|ccw|none` を追加しました。`cw` / `ccw`
では app 側の GLES 描画先を offscreen pbuffer にし、`SDL_GL_SwapWindow` 直前に
現在 bind されている framebuffer から texture へコピーします。Pyxel のように app 側が
内部 FBO を使い、Swap 時に default framebuffer へ戻していない場合もコピーを諦めません。
その後、物理 fb 用 EGL surface に切り替え、default framebuffer へ GPU 上でアスペクト比を
維持した回転 quad として描いてから swap します。`PLUMOS_A30MALI_LOGICAL_SIZE=720x480`
のように論理サイズが指定されている場合は、offscreen surface と present source にその値を
使います。これにより Pyxel のような標準 SDL2/GLES app を app 側 patch なしで A30 の
物理横画面へ合わせます。

実機の `plumos-sdl2-probe --gl-test` では以下を確認しました。

```text
video_driver index=0 name="a30mali"
sdl init=yes current_video_driver=a30mali
video_display index=0 name="1" bounds=0,0 480x640
gl vendor="ARM" renderer="Mali-400 MP"
gl swap=yes
```

この first pass は SDL window + GLES2 context + swap を優先した最小実装です。
Pyxel のように SDL2 window から `SDL_GL_CreateContext` へ進む app はこの経路に乗せられます。
一方、SDL_Renderer や汎用 window 管理を強く使う app は、必要に応じて backend 側の追加実装を
行います。
