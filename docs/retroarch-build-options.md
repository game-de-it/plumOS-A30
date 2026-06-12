# RetroArch build option 方針

調査日: 2026-06-07

この文書は、A30 stockOS の RetroArch build feature を観測し、それを plumOS の
RetroArch 実用 build option に落とすためのメモです。stockOS の version/options を
そのまま採用する表ではありません。RetroArch 本体は build 時点の upstream latest stable を
優先し、A30 実機で異常が出た場合だけ stockOS 側へ寄せるか比較します。libretro core は
この方針から分離し、Onion 採用 core catalog と実績 commit/build recipe に追従します。

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
| screenshots/images | rpng/rjpeg/screenshots | 手動 screenshot を gallery/scraping fallback に使う可能性があるため有効化する。 |
| content | `--enable-chd`, `--enable-flac` candidate | PS1/PCE CD/Mega CD/Neo Geo CD を残すなら CHD/CDDA 対応を検証する。 |
| info | core info cache optional | FE が launch profile を管理するため必須ではないが、core metadata には便利。 |

## 後回し候補

| group | feature | reason |
| --- | --- | --- |
| Netplay | Netplay | stock は有効だが、A30 Wi-Fi/CPUでは初期優先度を下げる。Network Command とは分ける。 |
| LibretroDB | database/playlists | FE 側で ROM scan/metadata を管理するため、初期 full runtime の必須ではない。 |
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
- minimal build の音声は `retroarch-minimal.cfg` で無効化していたため未検証

RetroArch 1.22.2 practical:

- `./scripts/docker-build.sh retroarch-practical` で ALSA/OSS audio、SDL2 joypad、Network Command、
  LibretroDB、7zip、screenshots/images を含む実用寄り runtime を build する。
- 2026-06-12 時点の A30 表示方針は `PLUMOS_RA_LANDSCAPE_MODE=fbo` を既定にする。
  RetroArch 側には `mali_fbdev` context の logical screen を 640x480 として報告し、
  Mali fbdev の native surface は物理 480x640 のまま保持する。GL2 frame/menu/OSD は
  640x480 の offscreen FBO に描画し、swap 前の present で 480x640 backbuffer へ90度回転して出す。
  FBO を毎フレーム clear した後は通常の `gl2_set_viewport()` を再適用し、RetroArch menu からの
  aspect ratio / integer scale 変更が game viewport に反映されるようにする。
  そのため practical config の `video_rotation` と `screen_orientation` はどちらも `0` にする。
- A30 の `mali_fbdev` context は threaded video 切替時に RetroArch restart を要求する。
  plumOS wrapper は `PLUMOS_RA_EXEC_PATH` / `PLUMOS_RA_LD_PATH` /
  `PLUMOS_RA_LIBRARY_PATH` / `PLUMOS_RA_CONFIG_PATH` /
  `PLUMOS_RA_RESTART_APPEND_PATH` を渡し、Unix frontend の restart 経路は restart 直前の
  `video_threaded` 実値を `/tmp/plumos-retroarch-restart.cfg` へ書き、
  `ld-linux --library-path ... retroarch.bin --config retroarch-practical.cfg --appendconfig ... --menu`
  で RGUI へ戻す。これは動的 loader を libretro core と誤認して落ちることと、
  `config_save_on_exit = false` により Threaded Video の ON/OFF が restart 直後に失われることを避けるため。
- 実機では `input_driver = "null"` + `input_joypad_driver = "sdl2"` +
  `plumos-joystickd --device-mode xbox` で `plumOS A30 Gamepad` が認識された。
- `udev`/`linuxraw` を primary input driver にした probe は初期化に失敗したため、
  初期実用 config は SDL2 joypad を優先する。
- OSS audio + SDL2 joypad + `audio_latency = 128` + CPU `performance` では画面/音/操作が
  ユーザー目視で良好だった。その後、System Settings の `volume` を全体音量として扱うため、
  既定は ALSA `default` + `Soft Volume Master` に移行し、OSS は互換 fallback として残す。
- A30 では fast-forward 開始時の一瞬停止を避けるため、RetroArch の
  `vrr_runloop_enable`（menu 表示名は `Sync to Exact Content Frame-rate (G-Sync, FreeSync)`）
  を初期 ON にする。実 VRR panel 前提ではなく、A30 の GL/vsync 切替で詰まる場面を
  runloop 側の frame limit に寄せるための A30 runtime default として扱う。既存の
  `retroarch-practical.cfg` は deploy 時に保護されるため、launcher append でも同じ値を渡す。
- CPU `ondemand` では音途切れが出た。CPU `userspace fixed 648000 kHz` + 2 cores は
  NES/GB で画面/音/操作が良好で、電池消費と安定性のバランス上、軽量 core の初期値にする。
- 電源ケーブルを抜いた状態の dummy CPU負荷計測では、2 cores + performance が約 1.98 W、
  4 cores + performance が約 8.07 W で、4 cores は約 4.1 倍の消費だった。
  そのため core count も system/ROM override 対象にする。
- FE の SELECT core menu には CPU preset と core count preset を表示し、system/ROM override から
  `plumos-retroarch-launch --cpu/--freq/--cores` へ解決する。
- `--enable-command` は practical runtime で有効化済み。A30 では `lo` がDOWNのことがあるため、
  launcher が RetroArch 起動前に `ip link set lo up` を試す。2026-06-08 時点で
  `GET_STATUS`/`QUIT` は `127.0.0.1:55355` 経由で確認済み。direct launcher 経路では
  `SAVE_STATE` -> `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` も通り、`CLOSE_CONTENT` 後に
  `GET_STATUS CONTENTLESS` を確認済み。launcher のTERM cleanupは
  `PLUMOS_RA_SAFE_EXIT=1` 既定で `SAVE_STATE_SLOT 999` -> `SAVE_FILES` ->
  `CLOSE_CONTENT` -> `QUIT` を試してからfallback killする。slot は
  `PLUMOS_RA_SAFE_STATE_SLOT` / `--safe-state-slot` で変更できる。
  `plumos-safe-shutdown --shutdown --no-poweroff` から FE/text-ui-owned launcher へTERMし、
  `resume-session.json` を `pending=true` / `auto_state_load=true` のまま保持する経路も
  A30実機で確認済み。controller UI の SAFE menu / START Shutdown から
  `plumos-safe-shutdown --no-poweroff` を呼ぶ経路も確認済み。`plumos-safe-hotkeyd` の
  `SIGUSR1` trigger では、RetroArch 実行中に同じ安全終了pathを通し、resume hold、
  CPU復元、FE再起動、残留なしまで確認済み。`plumos-text-ui launch --execute` からの
  `plumos-safe-hotkeyd --oneshot` 自動起動も確認済み。
  `scripts/probe-a30-safe-hotkeyd.sh --trigger signal|physical` で再実行できる。
  物理 trigger は Function から電源ボタン `KEY_POWER` へ移行した。2026-06-08 の
  `artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes`
  では `SAVE_STATE_SLOT 999`、`.state999` 作成、resume plan の `--entry-slot 999`
  まで確認済み。次はゲーム中SAFE menuを出すか直接安全終了にするかのUX決定。

## 次の実用 build の狙い

最初の practical runtime は、minimal video 経路を維持しつつ以下だけ増やすのがよいです。

- ALSA `default` と SDL2 joypad を初期実用 config とし、`plumos-volume-control` の
  `Soft Volume Master` を volume backend にする。
- OSS は互換 fallback として維持し、OSS 明示時だけ RetroArch software volume へ保存値を反映する。
- controller UI の SAFE menu/START Shutdown と `plumos-safe-hotkeyd` から接続済みの
  安全終了flowは、power/sleep backend dry-run まで確認済み。物理 trigger は電源ボタン
  `KEY_POWER` へ移行した。
  次は必要ならゲーム中SAFE menu、または実 poweroff / 実 suspend の発火確認へ広げる。
- CHD/FLAC は PS1/PCE CD/Mega CD/Neo Geo CD の core smoke 前に有効化して比較する。
