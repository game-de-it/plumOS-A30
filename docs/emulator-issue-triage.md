# Emulator Issue Triage

この文書は、全 core の A30 実機動作確認が一巡した後に、問題が残った profile を
原因調査と対策へ進めるための作業順を整理するものです。個別の検証結果の正本は
`docs/emulator-runtime-verification.tsv`、FE から実行可能な対象一覧は
`docs/emulator-fe-libretro-targets.tsv` と `docs/emulator-fe-standalone-targets.tsv` とします。

## 現在の集計

2026-06-18 時点の `emulator-runtime-verification.tsv` は以下の状態です。

| status | count | meaning |
| --- | ---: | --- |
| `pass` | 168 | video/audio/input/performance が実用範囲で確認済み |
| `pass_init` | 22 | 起動や初期表示は成立したが、gameplay/入力/音声/性能の追加確認余地あり |
| `fail` | 1 | 複合的に実用確認へ進めない |
| `fail_audio` | 3 | 起動/表示は成立するが音声が実用判定に届かない |
| `fail_boot` | 22 | FE から起動は試せるが content/game 起動へ進めない |
| `fail_input` | 6 | 表示や起動は成立するが入力が実用判定に届かない |
| `fail_perf` | 26 | 起動はするが性能、音声途切れ、frame pacing が実用判定に届かない |
| `fail_video` | 8 | 起動はするが画面崩れや表示異常がある |
| `retired` | 12 | 方針判断済みで通常 FE/動作確認対象から外した |
| `untested` | 4 | 必要 BIOS/ROM がなく未確認 |

## 切り分け方針

- RA と PICO の両方で同じ core が失敗する場合は、まず core recipe、BIOS/system path、
  content layout、core options を疑う。
- RA は成功し PICO だけ失敗する場合は、PicoArch frontend、RGB565 presenter、SDL1 input、
  frame pacing、または PicoArch 側の core option 初期化を疑う。
- performance 不足が明確で、同じ system に実用的な代替 core/profile がある場合は、
  修正よりも default から外す、または非推奨候補として残す判断を優先する。
- 既に standalone が正常動作している system は、libretro profile の修正優先度を下げる。
- 調査で改善できた場合は runtime TSV と FE target TSV を更新する。改善できない理由が確定した
  場合は `retired` または非 default として扱う。

## 解決済み

| group | affected profiles | resolution |
| --- | --- | --- |
| Atari 5200 / Atari 8-bit | `retroarch:atari800`, `picoarch:atari800` | `atari800` は system 別 core option が必要だった。RA は `Atari800-atari5200.opt` / `Atari800-atari800.opt` を起動時に選び、PICO は `PLUMOS_PICOARCH_SYSTEM` で cfg seed を切り替える。2026-06-18 direct smoke で `pass_init`。 |
| mGBA | GB/GBA の `retroarch:mgba`, `picoarch:mgba` | mGBA core の CMake source list から `version.c` と `src/platform/posix/memory.c` を外していたため、`projectName` / `anonymousMemoryMap` が未解決になっていた。不要な除外をやめ、PicoArch の library path は plumOS lib を system lib より先に見る順序へ変更。2026-06-18 direct smoke で GB/GBA は `pass_init`。GBC は実機に `.gbc` ROM がなく `untested`。 |

## 優先度 P1: system 全体が使えない、または代替が弱い問題

| group | affected profiles | first hypothesis | first action |
| --- | --- | --- | --- |
| Commodore 64 | `retroarch:frodo`, `retroarch:vice_x64`, `picoarch:frodo`, `picoarch:vice_x64` | RA/PICO 両方で video が壊れるため、core output format、machine/video standard、または build option 起因の可能性が高い。 | capture と RA log を取り、PAL/NTSC、VIC model、pixel format、core options を固定して再試験する。 |
| ChaiLove | `retroarch:chailove`, `picoarch:chailove` | RA log で SDL video device 初期化失敗が出ており、libretro core 内部で SDL window を開こうとしている可能性がある。 | core が pure libretro として動けるか確認し、無理なら通常候補から外す。 |
| Fairchild Channel F | `retroarch:freechaf`, `picoarch:freechaf` | BIOS/ROM format、mapper、または core option の可能性。 | freechaf の BIOS 要件と対象 ROM の header/extension を確認する。 |
| TIC-80 | `retroarch:tic80`, `picoarch:tic80` | content layout、core asset、または古い source/build 由来の起動問題。 | core info と log を確認し、`.tic` の読み込み失敗か runtime 初期化失敗か分ける。 |
| Wolfenstein 3D | `retroarch:ecwolf`, `picoarch:ecwolf` | ECWolf は data file set と directory layout の要求が強い。単体 file 起動ではなく data directory 起動が必要な可能性が高い。 | shareware data の配置と launcher が渡す content path を見直す。 |
| X68000 | `retroarch:px68k`, `picoarch:px68k` | BIOS path、`keropi` directory、または disk image 起動手順が不足している可能性が高い。 | `/mnt/SDCARD/Bios/keropi` と core system directory を確認し、RA log の missing BIOS を見る。 |
| ZX-81 | `retroarch:81`, `picoarch:81` | 起動はするが video/input が崩れるため、keyboard/joystick mode、model、display timing、rotation/scale の複合問題の可能性。 | core option と input mapping を固定し、まず RA 単体で video と input を分けて確認する。 |

## 優先度 P2: 代替はあるが、直ると選択肢が増える問題

| group | affected profiles | reason |
| --- | --- | --- |
| PicoArch fceumm | NES/FDS の `picoarch:fceumm` | RA fceumm と PICO nestopia/quicknes は動くため緊急度は低いが、PicoArch 起動引数や core option 問題の代表例になる。 |
| mame2003_plus | Arcade/Neo Geo/MAME 2003+ | ROM set 差と performance 問題が混ざっている。Arcade 系は RA 代替があるため、DAT/ROM set 確認を優先する。 |
| Neo Geo CD via FBNeo | `retroarch:fbneo`, `picoarch:fbneo` | `neocd` は通るため、FBNeo の media/BIOs/track layout 問題として扱う。 |
| PICO-8 | `retroarch:retro8`, `picoarch:retro8`, `retroarch:fake08` | `picoarch:fake08` は pass。BGM 異常と fake08 RA 起動失敗を分けて調べる。 |
| Cave Story | `retroarch:nxengine`, `picoarch:nxengine` | 日本語文字化けと PICO boot failure。data language/asset 対応問題の可能性が高い。 |
| Lutro / LowRes NX / VMU | `lutro`, `lowresnx`, `vemulator` | 画面差分系。pixel format、palette、presenter 経路の検証対象にする。 |

## 優先度 P3: PicoArch 共通層の問題

| symptom | affected profiles | direction |
| --- | --- | --- |
| analog axis rotation | `picoarch:hatari`, `picoarch:prboom`, `picoarch:dosbox_pure` | PicoArch launcher 側で `axisYR`/`axisXR` の rotated-axis default を適用済み。launcher/joystickd log では補正適用を確認済みで、実操作の目視確認待ち。 |
| lower FPS / frameskip flicker | Arcade PICO profiles, `picoarch:mame2003_plus` | RA との差分として audio sync、frame limiter、frameskip path、present timing を測る。 |
| PICO-only fail_boot | `picoarch:tyrquake`, `picoarch:lutro`, `picoarch:easyrpg`, `picoarch:squirreljme` など | PicoArch の content path、system dir、core option 初期化、working directory を RA と比較する。 |

## 優先度 P4: 性能限界として扱う候補

以下は A30 の CPU/GPU 性能に対して重い可能性が高く、代替がない場合でも深追いしすぎない。

- `opera` / 3DO
- `virtualjaguar` / Atari Jaguar
- `mednafen_pcfx` / PC-FX
- `uw8` / MicroW8
- `uzem` / Uzebox
- GBA の `mednafen_gba`, `meteor`, `vba_next`, `vbam`
- PC Engine の非 fast `mednafen_pce`
- RetroArch `scummvm`
- `mame2003_plus` の重い ROM

## 最初に実施する調査順

1. PicoArch 共通入力問題を調べる。
   `hatari`、`prboom`、`dosbox_pure` の axis rotation は per-core input transform でまとめて直せる可能性が高い。
2. C64 video corruption を調べる。
   RA/PICO 両方で壊れるため、core build/core option の切り分けに向いている。
3. BIOS/layout 系をまとめて見る。
   `px68k`、`ecwolf`、`freechaf`、`tic80` は missing BIOS/data layout/working directory の確認を優先する。
