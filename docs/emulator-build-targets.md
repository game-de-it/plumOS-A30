# plumOS emulator build targets

この文書は、Miyoo A30 stockOS の `/mnt/SDCARD/Emu`、`/mnt/SDCARD/RApp`、
`/mnt/SDCARD/RetroArch/.retroarch/cores` を参考に、plumOS でビルドする emulator /
libretro core / standalone runtime の候補を整理したものです。

stockOS の version を採用するための表ではありません。plumOS では build 時点の
upstream latest stable を初期候補にし、A30 実機で動作異常、性能退行、A30 固有不具合が
出た場合だけ stockOS 側の version、patch、build option へ寄せるか比較します。

調査日: 2026-06-07

## 方針

- `Emu/*` は stock MainUI の主要カテゴリとして扱う。
- `RApp/*` は追加/代替 core や追加カテゴリとして扱う。
- `RApp/backups/*` と RetroArch に入っているだけの core は、初期ビルド後の追加候補にする。
- PPSSPP は standalone を第一候補にし、libretro PPSSPP は比較/代替候補にする。
- FFPlay は emulator ではなく media player なので、emulator pack とは別枠で扱う。
- stock `launch.sh` は起動方式の参考にするが、plumOS の正式 launch profile にはしない。

## Runtime package targets

| package | plumOS target | note |
| --- | --- | --- |
| RetroArch | `/mnt/SDCARD/plumos/retroarch/bin/retroarch` | A30 armv7 hard-float 向けに build。input は SDL2/evdev + `plumos-joystickd --device-mode xbox` を優先。video は A30 の Mali/fbdev 実画面出力を別途検証する。 |
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | stockOS の core 名は参考のみ。可能な限り upstream latest stable を採用する。 |
| PPSSPP standalone | `/mnt/SDCARD/plumos/emulators/ppsspp/` | PSP の第一候補。`plumos-joystickd` 常駐との二重入力/serial 競合確認が必要。 |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | stock の `Emu/ffplay` 相当。video player なので emulator 初期セットとは分ける。 |

注意: plumOS 同梱 SDL3+sdl2-compat は A30 実画面 video backend を持たないことを確認済み。
RetroArch の実画面出力は、単純な SDL2 video 前提ではなく、fbdev/Mali EGL などの A30 向け
経路を検証してから決める。

## Tier 1: stock `Emu` categories

まずこの表を plumOS 初期 emulator/core build の基準にする。

| stock category | ROM dir | stock launch/core | plumOS build target | priority note |
| --- | --- | --- | --- | --- |
| ARCADE | `Roms/ARCADE` | `fbneo`, `fbalpha2012`, `mame2003_xtreme`, `mame` | `fbneo`, `fbalpha2012`, `mame2003-plus`, optional current `mame` | `fbneo` first。`mame` current は重い可能性が高く後回し。 |
| Shoot | `Roms/Shoot` | `fbalpha2012`, `fbneo`, `mame2003_xtreme` | `fbneo`, `fbalpha2012`, `mame2003-plus` | ARCADE と同じ arcade group で扱う。 |
| MAME2003PLUS | `Roms/MAME2003Plus` | `mame2003_xtreme` | `mame2003-plus` | stock の `xtreme` fork は比較対象。まず upstream 相当を試す。 |
| NEOGEO | `Roms/NEOGEO` | `fbneo` | `fbneo` | BIOS配置とgamelist互換も確認する。 |
| DC | `Roms/DC` | `flycast` | `flycast` | 高負荷/3D枠。video/input/audioが固まってから性能確認。 |
| FC | `Roms/FC` | `fceumm`, `nestopia` | `fceumm`, `nestopia` | 低負荷の初期 smoke test に向く。 |
| GB/GBC | `Roms/GB` | `gambatte`, `mgba` | `gambatte`, `mgba` | `gambatte` first、`mgba` はGB/GBC/GBA共通代替。 |
| GBA | `Roms/GBA` | `gpsp`, `mgba` | `gpsp`, `mgba` | `gpsp` performance first、`mgba` accuracy fallback。rumble launch は後回し。 |
| MD/SMS/GG/SG/32X | `Roms/MD`, `Roms/MS` | `genesis_plus_gx`, `picodrive` | `genesis_plus_gx`, `picodrive` | Mega Drive / Master System / Game Gear / SG-1000 / 32X / Mega CD をまとめて確認。 |
| N64 | `Roms/N64` | `mupen64plus`, `parallel_n64` | `mupen64plus-next`, optional `parallel-n64` | 高負荷/3D枠。A30では互換性と性能を厳しめに見る。 |
| NGP | `Roms/NGP` | `mednafen_ngp` | `mednafen_ngp` | 低負荷枠。 |
| PCE | `Roms/PCE` | `mednafen_pce_fast` | `mednafen_pce_fast` | PCE CD / SuperGrafx は追加枠も見る。 |
| PSP | `Roms/PSP` | standalone `PPSSPPSDL`, `ppsspp_xtreme_libretro` | PPSSPP standalone, optional `ppsspp_libretro` | standalone first。input daemon 競合確認が必要。 |
| PS1 | `Roms/PS` | `pcsx_rearmed` | `pcsx_rearmed` | BIOS/profile/save配置を早めに固める。 |
| SFC/SNES | `Roms/SFC` | `mednafen_supafaust`, `snes9x` | `mednafen_supafaust`, `snes9x`, optional `snes9x2005-plus` | stock default は supafaust。performance比較で default を決める。 |
| WSC | `Roms/WSC` | `mednafen_wswan` | `mednafen_wswan` | 低負荷枠。 |
| FFPlay | `Video` | `ffplay` | FFmpeg/FFPlay optional | emulator ではなく media player。別pack扱い。 |

## Tier 2: active `RApp` categories

stock のトップ `RApp` にある追加/代替カテゴリ。Tier 1 の次にビルド対象へ入れる。

| stock RApp | ROM dir | stock core | plumOS build target | note |
| --- | --- | --- | --- | --- |
| `dos` | `Roms/DOS` | `dosbox_pure` | `dosbox_pure` | DOS support。keyboard/input profile が別途必要。 |
| `easyrpg` | `Roms/EASYRPG` | `easyrpg` | `easyrpg` libretro or standalone EasyRPG Player | libretro first、必要なら standalone 比較。 |
| `fbahack` | `Roms/FBAHACK` | `fb_32b` | `fbneo`/FBA系比較 | stock固有寄り。互換性が必要なROMが出たら深掘り。 |
| `fbalpha2012_cps1/2/3` | `Roms/ARCADE/cps*` | `fbneo` or `fbalpha2012` | `fbneo`, `fbalpha2012` | ARCADE group の profile分けで扱う。 |
| `fbalpha2012_neogeo`, `fbneo` | `Roms/NEOGEO`, `Roms/ARCADE` | `fbneo` | `fbneo` | Tier 1 と同一core。 |
| `fceumm`, `nestopia` | `Roms/FC` | `fceumm`, `nestopia` | `fceumm`, `nestopia` | FC alternative profiles。 |
| `gambatte` | `Roms/GB` | `gambatte` | `gambatte` | GB/GBC profile。 |
| `genesis_plus_gx`, `genesis_plus_gx_gg` | `Roms/MD`, `Roms/GameGear` | `genesis_plus_gx` | `genesis_plus_gx` | Sega profile分け。 |
| `gpsp`, `mgba` | `Roms/GBA` | `gpsp`, `mgba` | `gpsp`, `mgba` | GBA profile。 |
| `mame2003_plus` | `Roms/MAME2003Plus` | `mame2003_xtreme` | `mame2003-plus` | stock fork比較は必要時のみ。 |
| `mednafen_ngp` | `Roms/NGP` | `mednafen_ngp` | `mednafen_ngp` | NGP profile。 |
| `mednafen_pce_fast` | `Roms/PCE` | `mednafen_pce_fast` | `mednafen_pce_fast` | PCE profile。 |
| `mednafen_supergrafx` | `Roms/SFC` in stock config | `mednafen_supafaust` | `mednafen_supafaust` | stock config 名は紛らわしい。SFC系として扱う。 |
| `mednafen_wswan` | `Roms/WSC` | `mednafen_wswan` | `mednafen_wswan` | WSC profile。 |
| `neocd` | `Roms/NEOCD` | `neocd` | `neocd` | Neo Geo CD support。BIOS確認が必要。 |
| `pcsx_rearmed` | `Roms/PS` | `pcsx_rearmed` | `pcsx_rearmed` | PS1 profile。 |
| `picodrive` | `Roms/MD` | `picodrive` | `picodrive` | 32X/Mega CD fallback。 |
| `prboom` | `Roms/PRBOOM` | `prboom` | `prboom` | Doom/WAD support。 |
| `retro8` | `Roms/PICO` | `retro8` | `retro8`, optional `fake08` standalone | stock dir also has `FAKE08`, but active launch uses `retro8_libretro.so`。 |
| `snes9x`, `snes9x2018` | `Roms/SFC` | `snes9x`, `snes9x2005_plus` | `snes9x`, optional `snes9x2005-plus` | performance fallback。 |

## Tier 3: backup/installed optional targets

stock SD には top-level ではない backup config や installed core もある。ROM directory が存在する、
またはユーザーが必要にした場合に追加する。

| family | candidate targets |
| --- | --- |
| MSX | `bluemsx` |
| Atari / classic | `handy` or `mednafen_lynx`, `prosystem`, `stella2014`, `potator`, `vecx`, `freechaf`, `o2em` |
| Nintendo extras | `gw`, `pokemini`, `quicknes`, `vba_next`, `mednafen_vb` |
| NEC computer / console | `np2kai`, `quasi88`, `mednafen_pcfx` |
| 3DO / Saturn | `opera`, `yabause`, `yabasanshiro`, optional `mednafen_saturn` |
| Adventure / engines | `scummvm`, `tic80`, `gme` |
| Other installed cores | `crocods`, `fake08`, `sameduck`, `pocketsnes`, `snes9x2002`, `snes9x2005`, `snes9x2010`, `mame2015`, `mame078plus`, `mame_libretro` |

## Suggested build order

1. RetroArch runtime skeleton and one low-risk core:
   `fceumm`, then video/audio/input smoke test.
2. Low-risk 2D core set:
   `gambatte`, `genesis_plus_gx`, `mednafen_pce_fast`, `mednafen_wswan`,
   `mednafen_ngp`, `snes9x` or `mednafen_supafaust`.
3. Common user-facing systems:
   `pcsx_rearmed`, `gpsp`, `mgba`, `fbneo`, `mame2003-plus`, `fbalpha2012`.
4. Standalone PSP:
   PPSSPP standalone with `plumos-joystickd --device-mode xbox`; check whether PPSSPP's extra
   `/dev/input/event3` and `/dev/ttyS0` opens cause duplicate input or serial contention.
5. High-load 3D:
   `flycast`, `mupen64plus-next`, optional `parallel-n64`.
6. Extra `RApp` and backup targets:
   `dosbox_pure`, `easyrpg`, `neocd`, `prboom`, `retro8`/`fake08`, then Tier 3 as needed.

## Open checks before building everything

- RetroArch real video output path on A30:
  plain upstream SDL2/SDL3+sdl2-compat is not enough for `/dev/fb0` display.
- Core build recipes need tag/URL/SHA-256/build options recorded in a manifest.
- Save/state/system/Bios directories must be moved away from stock `HOME=/mnt/SDCARD/RetroArch`.
- `plumos-joystickd --device-mode xbox` should be part of emulator launch profiles, not a stock
  `launch.sh` side effect.
- CPU governor/clock changes must be centralized in plumOS policy, not copied from stock scripts.
