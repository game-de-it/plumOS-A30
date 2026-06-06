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
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | stockOS の core 名は参考のみ。`fceumm` と `gambatte` は upstream HEAD から build/deploy 済み。以後も可能な限り upstream latest stable/HEAD を採用する。 |
| standalone emulators | `/mnt/SDCARD/plumos/emulators/<id>/` | PPSSPP や必要な standalone engine を置く。libretro より適切な場合だけ採用する。 |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | stock の `Emu/ffplay` 相当。video player なので emulator 初期セットとは分ける。 |

注意: plumOS 同梱 SDL3+sdl2-compat は A30 実画面 video backend を持たないことを確認済み。
RetroArch minimal display probe は SDL を使わず、RetroArch の `mali_fbdev` context と
A30 rootfs の `/usr/lib/libEGL.so`/`libGLESv2.so` を使う。RGUI 表示は確認済みだが、
core を読み込んだ実ゲーム画面は `fceumm`/`gambatte` で確認済みです。現在の
`retroarch-minimal.cfg` は audio disabled のため、音声と emulator-facing input は次段階で検証します。

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
| PS1 | `pcsx_rearmed` | `Emu/PS`, `RApp/pcsx_rearmed` | A30でも現実的。BIOS/save/state配置を早めに固める。 |
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
| ScummVM | `scummvm` | installed core | 低負荷寄り。マウス/キーボード操作profileが必要。 |
| EasyRPG | `easyrpg`, optional standalone EasyRPG Player | `RApp/easyrpg` | 実用候補。 |
| DOS classics | `dosbox_pure` | `RApp/dos` | 軽量DOS game限定。keyboard/profileが必要。 |
| MSX | `bluemsx` | backup `bluemsx` | stock top-levelではないがA30実用性では候補。 |

## Class B: 検証後に初期対象へ入れる候補

A30で動く可能性はあるが、満足ラインはtitle/profile依存。

| system / ROM family | launch profile candidates | reason |
| --- | --- | --- |
| CPS3 | `fbalpha2012`, `fbneo` | 2Dだが重め。代表タイトルで実測して判断。 |
| SNES enhancement-chip titles | `snes9x`, `snes9x2005-plus`, `mednafen_supafaust` | SA-1/SuperFX等は個別性能確認。 |
| PC-88 / PC-98 | `quasi88`, `np2kai` | CPU負荷より入力/keyboard UXが課題。必要なら build。 |
| Virtual Boy | `mednafen_vb` | 動作は見込めるが画面/UXが特殊。 |
| lightweight PSP | PPSSPP standalone | PSP全体ではなく、2D/軽量タイトル限定で検証。 |
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

## Suggested build order

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

## Open checks before building everything

- RetroArch audio/input smoke: minimal RGUI and `fceumm`/`gambatte` core-loaded
  video are confirmed through `fbdev_mali`, but audio and emulator-facing input
  still need device validation.
- Core build recipes need tag/URL/SHA-256 or commit/build options recorded in a manifest.
- Save/state/system/Bios directories must be moved away from stock `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch profiles, not a stock
  `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied from stock scripts.
