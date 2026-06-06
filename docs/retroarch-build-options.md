# RetroArch build option 方針

調査日: 2026-06-07

この文書は、A30 stockOS の RetroArch build feature を観測し、それを plumOS の
RetroArch 実用 build option に落とすためのメモです。stockOS の version/options を
そのまま採用する表ではありません。plumOS では build 時点の upstream latest stable を
優先し、A30 実機で異常が出た場合だけ stockOS 側へ寄せるか比較します。

## Source

- RetroArch upstream: <https://github.com/libretro/RetroArch>
- FCEUmm upstream: <https://github.com/libretro/libretro-fceumm>
- Gambatte upstream: <https://github.com/libretro/gambatte-libretro>
- Core info upstream: <https://github.com/libretro/libretro-core-info>

## StockOS RetroArch feature

A30 実機の `/mnt/SDCARD/RetroArch/retroarch --version`:

```text
RetroArch 1.16.0 Apr 25 2024
Compiler: GCC (7.5.0) 32-bit
MIYOO Build:Apr 25 2024 22:32:35
```

`--features` で見えた主な有効機能:

```text
SDL yes
SDL2 no
Threads yes
OpenGL yes
OpenGLES yes
EGL yes
ALSA yes
OSS yes
7zip yes
zlib yes
rpng yes
rjpeg yes
Dynamic yes
Network Command yes
Netplay yes
LibretroDB yes
GLSL yes
```

主な無効機能:

```text
X11/Wayland/KMS/Vulkan/Metal no
UDEV no
FFmpeg no
FreeType no
SDL_image no
PulseAudio/Jack/OpenAL/OpenSL no
Libusb no
Qt/Cocoa no
Video4Linux2 no
```

## plumOS で残す候補

| group | option / feature | reason |
| --- | --- | --- |
| core loading | `--enable-dylib` | libretro core を動的 load するため必須。 |
| video | `--enable-opengles`, `--enable-egl`, `--enable-mali_fbdev` | A30 の実画面は `fbdev_mali` + GLES/EGL で確認済み。 |
| video | `--enable-rgui` | 軽量で A30 画面に収まりやすい。最初の menu driver とする。 |
| shader | GLSL enabled | minimal build でも GLSL を切ると GL 初期化に失敗した。 |
| CPU | `--enable-neon`, `--enable-floathard`, `--enable-threads` | A30 は ARMv7 NEON/VFPv4 hard-float。 |
| archive | `--enable-zlib`, `--enable-builtinzlib`, `--enable-7zip` | NES/GB ROM set に zip があり、将来の compressed content でも便利。 |
| remote control | `--enable-networking`, `--enable-command` | shutdown/resume flow から save/quit を安全に送る候補。 |
| audio | `--enable-oss` first, `--enable-alsa` if sysroot/deps are stable | stock は OSS/ALSA 両方有効。A30 実機では音声 device の掴み方を検証する。 |
| input | `--enable-sdl2` and/or `--enable-udev` candidate | stock SDL1 ではなく、plumOS は `plumos-joystickd --device-mode xbox` と SDL2/evdev を優先する。 |
| content | `--enable-chd`, `--enable-flac` candidate | PS1/PCE CD/Mega CD/Neo Geo CD を残すなら CHD/CDDA 対応を検証する。 |
| info | core info cache optional | FE が launch profile を管理するため必須ではないが、core metadata には便利。 |

## 後回し候補

| group | feature | reason |
| --- | --- | --- |
| Netplay | Netplay | stock は有効だが、A30 Wi-Fi/CPUでは初期優先度を下げる。Network Command とは分ける。 |
| LibretroDB | database/playlists | FE 側で ROM scan/metadata を管理するため、初期 full runtime の必須ではない。 |
| screenshots/images | rpng/rjpeg/screenshots | 画面確認や thumbnail 用に便利だが、まずは audio/input を優先。 |
| menu | Ozone/XMB/MaterialUI | A30では RGUI を基本にする。見た目改善は FE 側で行う。 |
| rewind/runahead | rewind, runahead | A30 の CPU/RAM では初期無効でよい。 |
| softpatch | patch/xdelta | IPS/BPS が必要になったら戻す。 |

## 無効化する候補

| group | feature | reason |
| --- | --- | --- |
| desktop/windowing | X11, Wayland, KMS, GLX, XVideo, Vulkan, Metal, Qt, Cocoa | A30 stockless plumOS の実画面経路では不要。 |
| SDL1 | `--disable-sdl` | stock SDL1 に寄せず、plumOS の入力設計に合わせる。 |
| heavy media | FFmpeg, V4L2, camera, microphone | emulator runtime の初期用途では不要。 |
| desktop audio | PulseAudio, PipeWire, Jack, OpenAL, RSound, RoarAudio | A30 rootfs に合わせない。OSS/ALSA を先に検証する。 |
| HID extras | libusb, blissbox, parport, bluetooth | A30 builtin buttons/joystickd を優先する。 |
| online updater | update cores/assets/info, SSL, Discord | plumOS package 管理で扱い、実機から更新しない。 |
| shader stack | Cg, HLSL, Slang, glslang, SPIRV-Cross | GLES2/Mali-400 では GLSL 以外は不要。 |
| EGL loading | `--enable-dynamic_egl` | A30 minimal probe で `eglGetDisplay` fallback 後に SIGSEGV したため使わない。 |

## 現在の確認結果

RetroArch 1.22.2 minimal:

- `video_driver = "gl"`
- `video_context_driver = "mali_fbdev"`
- `PLUMOS_RA_DISPLAY_ROTATION=ccw`
- A30 GL2 menu MVP patch 適用
- RGUI 横向き表示をユーザー目視確認済み

libretro core smoke:

```sh
./scripts/docker-build.sh libretro-cores
A30_TARGET=root@192.168.10.165 ./scripts/probe-a30-libretro-cores.sh --duration 6
```

- `fceumm` commit: `3a84a6fd0ba20dd4877c06b1d58741172148395f`
- `gambatte` commit: `6716e6ee39c2abd3ea325f66fb26e7f866f4c5dc`
- `core-info` commit: `f8b241d098e74c668001bb0aba8129b40afe7d03`
- `fceumm` + NES: ROM load、save path生成、Mali fbdev表示 loop 生存確認済み
- `gambatte` + GB: ROM load、Mali fbdev表示 loop 生存確認済み
- ユーザー目視で両方のゲーム画面表示を確認済み
- 音声は `retroarch-minimal.cfg` で無効化しているため未検証

## 次の実用 build の狙い

最初の full runtime は、minimal video 経路を維持しつつ以下だけ増やすのがよいです。

- OSS audio を有効にした build と config で短時間音出しを確認する。
- ALSA は `libasound` と実機 device が安定して扱える場合だけ併用または比較する。
- `plumos-joystickd --device-mode xbox` を起動し、SDL2/udev または linuxraw で RetroArch
  input が読めるか確認する。
- `--enable-command` を有効にし、FE から `QUIT`/save-state 系の安全終了を試す。
- CHD/FLAC は PS1/PCE CD/Mega CD/Neo Geo CD の core smoke 前に有効化して比較する。
