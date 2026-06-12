# plumOS emulator build targets

この文書は、Miyoo A30 stockOS の `/mnt/SDCARD/Emu`、`/mnt/SDCARD/RApp`、
`/mnt/SDCARD/RetroArch/.retroarch/cores` を参考にしつつ、plumOS でビルドする
emulator / libretro core / standalone runtime を A30 の実用性で整理したものです。

stockOS のカテゴリや version を採用するための表ではありません。plumOS FE では stock
MainUI の「カテゴリ」は設計軸にせず、機種/ROM集合に対して SELECT で core/emulator
launch profile を選ぶ前提にします。

plumOS では build 時点の upstream latest stable を初期候補にし、A30 実機で動作異常、
性能退行、A30 固有不具合が出た場合だけ stockOS 側の version、patch、build option へ
寄せるか比較します。

調査日: 2026-06-07

## 方針

- stock `Emu/*`、`RApp/*`、installed core は build target の発見元として扱う。
- plumOS の主分類は「カテゴリ」ではなく system と launch profile にする。
- A30で満足に遊べる見込みが低い system は、stockOSに存在しても初期ビルド対象から外す。
- CD media 系は一律除外しない。PS1、PC Engine CD、Mega CD、Neo Geo CD は現実的な候補に残す。
- PPSSPP は PSP全体対応ではなく、軽量タイトル向けの実験/限定対応から始める。
- FFPlay は emulator ではなく media player なので、emulator pack とは別枠にする。
- stock `launch.sh` は起動方式の参考にするが、plumOS の正式 launch profile にはしない。

## Runtime package targets

| package | plumOS target | note |
| --- | --- | --- |
| RetroArch | `/mnt/SDCARD/plumos/retroarch/bin/retroarch` | RetroArch 1.22.2 minimal RGUI build で GLES/EGL + `fbdev_mali` の実画面表示を確認済み。A30 の横向き RGUI は GL2 menu MVP patch + CCW 90度で表示する。`fceumm`/`gambatte` の core-loaded game screen も確認済み。full runtime では audio/input の追加検証が必要。input は SDL2/evdev + `plumos-joystickd --device-mode xbox` を優先。 |
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | stockOS の core 名は参考のみ。Class A/B の 41 core は upstream HEAD 中心で A30 armv7 hard-float 向けに build 済み。成果物は `dist/plumos-libretro-cores`。A30 実機では `fceumm` と `gambatte` の画面表示のみ確認済みで、残りは system ごとの起動/performance/input/audio 検証が必要。 |
| standalone emulators | `/mnt/SDCARD/plumos/emulators/<id>/` | PPSSPP、ScummVM、EasyRPG Player、DOSBox Staging、PCSX-ReARMed の試作 build は `dist/plumos-standalone-emulators` に stage 済み。A30 実機検証後、PPSSPP/ScummVM/EasyRPG Player/PCSX-ReARMed は standalone profile 候補へ昇格し、DOSBox Staging は通常対象外にする。 |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | stock の `Emu/ffplay` 相当。video player なので emulator 初期セットとは分ける。 |

注意: plumOS 同梱 SDL3+sdl2-compat は A30 実画面 video backend を持たないことを確認済み。
RetroArch minimal display probe は SDL を使わず、RetroArch の `mali_fbdev` context と
A30 rootfs の `/usr/lib/libEGL.so`/`libGLESv2.so` を使う。RGUI 表示は確認済みだが、
core を読み込んだ実ゲーム画面は `fceumm`/`gambatte` で確認済みです。現在の
`retroarch-minimal.cfg` は audio disabled のため、音声と emulator-facing input は次段階で検証します。

## Build status

2026-06-07 時点で `./scripts/docker-build.sh libretro-cores` により Class A/B の 41 core を
`dist/plumos-libretro-cores/plumos/retroarch/cores` に stage 済みです。`manifest.txt` は
実成果物ベースで `built=41`, `failed=0`, `skipped=0` です。

`vecx` は upstream HEAD の既定 build が OpenGL header を要求したため、A30 の fbdev/Mali/SDL
前提に合わせて `platform=armv HAS_GPU=0` で software path として build しました。
ScummVM は libretro Makefile の lite 構成、EasyRPG は最初の libretro build として
ICU/XML/一部音声補助を外した最小寄り構成です。これらは「build 済み」であり、実用性は
A30 実機の title/profile 別検証で判断します。

standalone emulator は `./scripts/docker-build.sh standalone-emulators` で
`dist/plumos-standalone-emulators` に stage します。2026-06-07 時点の試作 build は
`PPSSPP v1.20.4`、`ScummVM v2026.2.0`、`EasyRPG Player 0.8.1.1`、
`DOSBox Staging v0.82.2`、`PCSX-ReARMed r26l` の 5 本で、manifest は `built=5`,
`failed=0`, `skipped=0` です。ScummVM は classic engine subset、DOSBox Staging は
cross build 用に system SpeexDSP と fixed page size を使い、PCSX-ReARMed standalone は
upstream `miyoo` platform ではなく generic SDL1 frontend + ARMv7/NEON で build しています。
2026-06-08 の A30 実機検証では、PPSSPP、ScummVM、EasyRPG Player、PCSX-ReARMed は
画面、音、入力、メニュー/終了動線、config/save path の first-pass 確認を完了しました。
DOSBox Staging も横向き表示と入力は成立しましたが、実ゲーム負荷と音切れの面で
DOSBox-Pure libretro より不利なため、通常 profile には入れません。

## Standalone adoption snapshot

| emulator | decision | A30 result |
| --- | --- | --- |
| PPSSPP v1.20.4 | lightweight PSP 用の standalone default 候補 | 全体回転 + scissor回避 + `854x480` logical UI、L2 menu、A30 gamepad、固定CPU 1344MHz/4core を確認。PSP全体対応ではなく軽量title限定。 |
| ScummVM v2026.2.0 | ScummVM system の standalone default 候補 | `rotation_mode=270`、VirtualMouse warp修正、A30向け theme `scummmodern-a30-md` で画面/マウス/音/終了OK。 |
| EasyRPG Player 0.8.1.1 | EasyRPG system の standalone default 候補 | MP3/mpg123、Vorbis/Opus/MOD/LZH/Freetype+Harfbuzz 等を有効化し、音声/入力/終了OK。 |
| PCSX-ReARMed r26l | PS1 system の standalone default 候補 | native fb32 rotation、menu 640x480 landscape virtual、Function menu open/return、shadow clear、入力/音/ゲーム画面OK。 |
| DOSBox Staging v0.82.2 | 通常対象外、検証アーティファクト扱い | SDL2/mali で表示/入力は成立するが、主処理が1コア上限に近く音切れしやすい。DOSは `retroarch:dosbox_pure` を標準にする。 |
| Red Viper | Virtual Boy 用 standalone 候補 | A30 で ARM dynarec は有効。2026-06-12 の実機 probe では headless 322.86fps、software VIP render 込みでも 1344MHz/4core で 289.00fps、648MHz/2core で 135.13fps。`red-viper-a30` wrapper は fbdev 表示、A30 input、ALSA 音声、終了 cleanup を持つ。 |

## Virtual Boy note

Virtual Boy の `mednafen_vb`/Beetle VB は A30 では CPU 側が律速になります。2026-06-12 の
実機測定では、1344MHz/4core でも FBO 表示ありで約23fps、null video/audio でも約25fps
でした。core option の `vb_cpu_emulation=fast` は既に有効で、3D 表示 mode の変更や音声停止
だけでは 50fps に届きません。

Red Viper は libretro core ではなく 3DS 由来の standalone emulator ですが、A30 の armv7
hard-float で ARM dynarec が動きます。zip から一時展開した raw `.vb` ROM を使った実験用
静的リンク probe の結果は以下です。

| condition | result |
| --- | --- |
| headless, no audio, no display, 1344MHz/4core, 1200 frames + 120 warmup | 322.86fps |
| software VIP render, no audio, 1344MHz/4core, 600 frames + 60 warmup | 289.00fps |
| software VIP render, no audio, 648MHz/2core, 600 frames + 60 warmup | 135.13fps |

したがって、Virtual Boy の性能問題は Red Viper で解消できる見込みがあります。
`red-viper-a30` wrapper は Red Viper の software framebuffer を A30 の `/dev/fb0` へ
横向き表示し、A30 物理 input と Function/START+SELECT 終了を扱います。FE は
`standalone:red_viper` を Virtual Boy の default profile にし、`retroarch:mednafen_vb` を
fallback として残します。A30 wrapper は 3DS 版 Red Viper の menu UI は持たず、
設定は `plumos/config/standalone/red_viper.env` と FE launch profile で扱います。
単眼画面向けに `PLUMOS_A30_RED_VIPER_EYE=both` を既定にし、左右目を赤成分の最大値で合成します。
音声は上流 `vb_sound.c` と ALSA `default` 出力で有効化しています。A30 の実 PCM は
48kHz で開かれるため、Red Viper A30 build では `RED_VIPER_A30_AUDIO_RATE=48000`
を既定にし、ALSA latency 160ms、起動時 prebuffer、10ms chunk の producer queue を使います。
producer queue が空になった場合は直前の audio chunk を継続して、ROM 側の一時的な音声生成遅れで
ALSA buffer が空になることを避けます。上流既定の 50kHz のままでは音飛びと映像のカクつきが
出ました。`plug:default` による変換は A30 の libasound で assert するため使いません。
2026-06-13 の実機確認では、48kHz build でゲーム本編の音声/映像が正常に見えることを
確認しています。7z 展開は後続課題です。
Red Viper/Reality Boy 系の license は配布前に third-party notice と利用条件を確認します。

## Class A: 初期ビルド対象

A30で満足に動く可能性が高く、plumOSの初期対応に入れる候補。

| system / ROM family | launch profile candidates | stock discovery | note |
| --- | --- | --- | --- |
| FC / NES / FDS | `fceumm`, `nestopia`, optional `quicknes` | `Emu/FC`, `RApp/fceumm`, `RApp/nestopia`, backup `quicknes` | 最初のsmoke test向き。 |
| GB / GBC | `gambatte`, optional `mgba` | `Emu/GB`, `RApp/gambatte`, backup `gambatte_color` | 低負荷。 |
| GBA | `gpsp`, `mgba`, optional `vba_next` | `Emu/GBA`, `RApp/gpsp`, `RApp/mgba`, backup `vba_next` | `gpsp` performance first、`mgba` accuracy fallback。 |
| Master System / Game Gear / SG-1000 | `genesis_plus_gx` | `Emu/MS`, `RApp/genesis_plus_gx_gg`, backup `genesis_plus_gx_ms/sg` | MD系profileと共通化できる。 |
| Mega Drive / Genesis | `genesis_plus_gx`, `picodrive` | `Emu/MD`, `RApp/genesis_plus_gx`, `RApp/picodrive` | 32X/Mega CDは別profile扱い。 |
| Mega CD / Sega CD | `genesis_plus_gx`, `picodrive` | `RApp/picodrive`, backup `picodrive_cd` | CD系だがA30でも現実的な候補。 |
| 32X | `picodrive` | `Emu/MD`, `RApp/picodrive` | title-dependent寄りだが、軽量なものは候補に残す。 |
| SFC / SNES | `snes9x`, `mednafen_supafaust`, optional `snes9x2005-plus` | `Emu/SFC`, `RApp/snes9x`, `RApp/snes9x2018` | enhancement chip title は個別検証。 |
| PC Engine / TurboGrafx-16 | `mednafen_pce_fast` | `Emu/PCE`, `RApp/mednafen_pce_fast` | 低負荷。 |
| PC Engine CD | `mednafen_pce_fast` | backup `mednafen_pce_cd` | PS1以外のCD系では有力。BIOS/CHD/CUE確認が必要。 |
| SuperGrafx | `mednafen_supergrafx` or `mednafen_pce_fast` | backup `mednafen_supergrafx` | 実用候補。 |
| Neo Geo cartridge | `fbneo` | `Emu/NEOGEO`, `RApp/fbneo` | BIOS/gamelist互換確認が必要。 |
| Neo Geo CD | `neocd` | `RApp/neocd` | CD系だが2D中心で候補に残す。ロード/CDDA確認が必要。 |
| Arcade 2D | `fbneo`, `fbalpha2012`, `mame2003-plus` | `Emu/ARCADE`, `Emu/Shoot`, `RApp/fbneo`, `RApp/mame2003_plus` | CPS1/CPS2/Neo Geo/古めのMAMEを主対象にする。 |
| PS1 | `standalone:pcsx_rearmed`, `retroarch:pcsx_rearmed` | `Emu/PS`, `RApp/pcsx_rearmed` | A30でも現実的。standalone は実機検証済みで初期default候補。BIOS/save/state配置を固める。 |
| NGP / NGPC | `mednafen_ngp` | `Emu/NGP`, `RApp/mednafen_ngp` | 低負荷。 |
| WonderSwan / WonderSwan Color | `mednafen_wswan` | `Emu/WSC`, `RApp/mednafen_wswan` | 低負荷。 |
| Atari Lynx | `mednafen_lynx` or `handy` | backup `mednafen_lynx`, installed `handy` | stock top-levelではないが、A30実用性では初期候補に入る。 |
| Atari 2600 / 7800 | `stella2014`, `prosystem` | installed cores | 低負荷。stock top-levelではないが候補。 |
| Vectrex / Watara Supervision / Odyssey2 | `vecx`, `potator`, `o2em` | installed cores | 低負荷/ニッチ枠。 |
| Game & Watch | `gw` | backup `gw` | 低負荷。 |
| Pokemon Mini | `pokemini` | backup `pokemini` | 低負荷。 |
| Doom / WAD | `prboom` | `RApp/prboom` | 実用候補。 |
| PICO-8 carts | `retro8`, optional standalone `fake08` | `RApp/retro8`, installed `fake08` | active launch は `retro8`; standalone `fake08` は比較候補。 |
| TIC-80 | `tic80` | backup `tic80` | stock top-levelではないが実用候補。 |
| ScummVM | `standalone:scummvm`, optional `retroarch:scummvm` | installed core | standalone は回転/マウス/theme 調整済みで初期default候補。 |
| EasyRPG | `standalone:easyrpg`, optional `retroarch:easyrpg` | `RApp/easyrpg` | standalone は音声補助機能込みで実用候補。 |
| DOS classics | `retroarch:dosbox_pure` | `RApp/dos` | 軽量DOS game限定。standalone DOSBox Staging は通常対象外。keyboard/profileが必要。 |
| MSX | `bluemsx` | backup `bluemsx` | stock top-levelではないがA30実用性では候補。 |

## Class B: 検証後に初期対象へ入れる候補

A30で動く可能性はあるが、満足ラインはtitle/profile依存。

| system / ROM family | launch profile candidates | reason |
| --- | --- | --- |
| CPS3 | `fbalpha2012`, `fbneo` | 2Dだが重め。代表タイトルで実測して判断。 |
| SNES enhancement-chip titles | `snes9x`, `snes9x2005-plus`, `mednafen_supafaust` | SA-1/SuperFX等は個別性能確認。 |
| PC-88 / PC-98 | `quasi88`, `np2kai` | CPU負荷より入力/keyboard UXが課題。必要なら build。 |
| Virtual Boy | `standalone:red_viper`, fallback `retroarch:mednafen_vb` | Beetle VB は A30 で約25fps止まり。Red Viper dynarec は software render 込みでも 135fps 以上を確認済み。A30 standalone wrapper は表示/入力/ALSA 音声/終了 cleanup を持つ。 |
| lightweight PSP | `standalone:ppsspp` | PSP全体ではなく、2D/軽量タイトル限定で検証。A30向け入力/menu/表示は first-pass OK。 |
| old computer engines | `crocods`, `gme`, other installed cores | ROM需要と入力profile次第。 |

## Class C: 初期ビルド対象外

A30で満足に遊ぶには厳しい可能性が高いもの。stockOSに存在しても、初期plumOSでは
build/test工数を割かない。ユーザー要望や明確な軽量titleがある場合だけ実験枠にする。

| system / ROM family | stock/core hint | reason |
| --- | --- | --- |
| Dreamcast | `flycast` | A30のCPU/GPU性能では常用満足ラインが厳しい見込み。 |
| Nintendo 64 | `mupen64plus`, `parallel_n64` | 一部軽量title以外は厳しい見込み。初期対象外。 |
| Sega Saturn | `yabause`, `yabasanshiro`, `mednafen_saturn` | A30では重い。 |
| 3DO | `opera` | A30では重い見込み。 |
| PC-FX | `mednafen_pcfx` | A30では重い見込み。 |
| current MAME / newer arcade 3D | `mame`, `mame2015` | 古い2D arcadeはClass Aだが、新しいMAME系は後回し。 |
| heavy PSP | PPSSPP standalone/libretro | PSP全体対応とはしない。軽量titleだけClass B。 |

## Suggested validation order

Build 自体は Class A/B の 41 core が完了済みです。次は deploy 後の実機検証を以下の順で進めます。

1. RetroArch runtime skeleton and first low-risk cores:
   `fceumm` and `gambatte` are build/deploy/screen-smoke confirmed.
2. Audio/input smoke for the practical RetroArch runtime:
   OSS/ALSA selection, SDL2/evdev or linuxraw input, and `plumos-joystickd --device-mode xbox`.
3. Low-risk 2D set:
   `genesis_plus_gx`, `mednafen_pce_fast`, `mednafen_wswan`,
   `mednafen_ngp`, `snes9x`, `fbneo`.
4. Common handheld/console set:
   `gpsp`, `mgba`, `pcsx_rearmed`, `picodrive`, `mame2003-plus`, `fbalpha2012`.
5. CD systems that are still realistic:
   PS1 via `pcsx_rearmed`, PC Engine CD via `mednafen_pce_fast`,
   Mega CD via `genesis_plus_gx`/`picodrive`, Neo Geo CD via `neocd`.
6. stock backup/installed core から繰り上げる軽量system:
   `bluemsx`, `mednafen_lynx`/`handy`, `stella2014`, `prosystem`, `vecx`,
   `potator`, `gw`, `pokemini`, `tic80`, `scummvm`, `easyrpg`, `prboom`,
   `dosbox_pure`, `retro8`/`fake08`.
7. Conditional checks:
   CPS3, SNES enhancement-chip titles, PC-88/PC-98, lightweight PSP.

## Open checks after bulk build

- RetroArch audio/input smoke: practical runtime では OSS audio + SDL2 joypad +
  `plumos-joystickd --device-mode xbox` を NES/GB で確認済み。現在の既定は
  ALSA `default` + `Soft Volume Master` + SDL2 joypad だが、bulk build core 全体では未検証。
- Core build recipes are recorded in `dist/plumos-libretro-cores/docs/manifest.txt`; raw per-core details from the doubled run are kept as `manifest.raw-double-run.txt`.
- Standalone emulator build recipes are recorded in `dist/plumos-standalone-emulators/docs/manifest.txt`.
- PPSSPP/ScummVM/EasyRPG/PCSX-ReARMed standalone package は A30 first-pass validation 済み。ScummVM directory ROM は `.plumos-scummvm-target`、`scummvm-target.txt`、`.scummvm`、隣接 `.scummvm`/`.svm` sidecar から target id を解決し、無い場合だけ `sky` に fallback する。DOSBox Staging standalone は検証済みだが通常対象外。
- DOSBox-Pure libretro は `core-overrides.json` の per-ROM profile で
  `#EXE` suffix、ALSA/OSS audio driver、audio latency、`dosbox_pure_force60fps`、
  `dosbox_pure_cycles`、CPU policy/frequency/core count を保存できる。A30では
  `DOS/DOSBOX_DIGGER.ZIP` に `#DIGGER.EXE`、OSS latency 256、force60fps、cycles max、
  CPU fixed 1344MHz/4core を保存し、FE Aボタン経路から `execute: ok` まで確認済み。
- Save/state/system/Bios directories must be moved away from stock `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch profiles, not a stock
  `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied from stock scripts.
