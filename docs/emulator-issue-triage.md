# Emulator Issue Triage

この文書は、全 core の A30 実機動作確認が一巡した後に、問題が残った profile を
原因調査と対策へ進めるための作業順を整理するものです。個別の検証結果の正本は
`docs/emulator-runtime-verification.tsv`、FE から実行可能な対象一覧は
`docs/emulator-fe-libretro-targets.tsv` と `docs/emulator-fe-standalone-targets.tsv` とします。

## 現在の集計

2026-06-19 時点の `emulator-runtime-verification.tsv` は以下の状態です。

| status | count | meaning |
| --- | ---: | --- |
| `pass` | 168 | video/audio/input/performance が実用範囲で確認済み |
| `pass_init` | 36 | 起動や初期表示は成立したが、gameplay/入力/音声/性能の追加確認余地あり |
| `fail` | 1 | 複合的に実用確認へ進めない |
| `fail_audio` | 3 | 起動/表示は成立するが音声が実用判定に届かない |
| `fail_boot` | 12 | FE から起動は試せるが content/game 起動へ進めない |
| `fail_input` | 4 | 表示や起動は成立するが入力が実用判定に届かない |
| `fail_perf` | 26 | 起動はするが性能、音声途切れ、frame pacing が実用判定に届かない |
| `fail_video` | 4 | 起動はするが画面崩れや表示異常がある |
| `retired` | 12 | 方針判断済みで通常 FE/動作確認対象から外した |
| `untested` | 6 | 必要 BIOS/ROM がなく未確認 |

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
| Commodore 64 | `retroarch:frodo`, `retroarch:vice_x64`, `picoarch:frodo`, `picoarch:vice_x64` | `vice_x64` は ROM なし起動と最小 `.prg` autostart で RA/PICO とも正常表示。`frodo` は最小 `.p00` で RA/PICO とも C64 BASIC 画面を正常表示。Pacmania.zip は中身が `.tap` で、`frodo` は TAP 非対応、`vice_x64` は `Datasette: Cannot read in tap-file` を出すため、画面破損ではなく検証 ROM 形式/内容の相性問題として `pass_init` に戻した。capture は `artifacts/c64-video-20260618-234134-vice-nogame/`、`artifacts/c64-video-20260618-234449-vice-prg/`、`artifacts/c64-video-20260618-234759-frodo-p00/`。 |
| X68000 | `retroarch:px68k`, `picoarch:px68k` | 実機の `/mnt/SDCARD/Bios/keropi` は `IPLROM.DAT` / `CGROM.DAT` のような大文字名で、PX68k core は小文字名だけを探していた。`px68k-libretro-uppercase-bios.patch` で大文字 BIOS 名も候補に入れ、RA/PICO とも `CH68_110.xdf` で Human68k boot 画面まで到達。capture は `artifacts/layout-triage-20260619-000721-px68k-ra-corepatch/`、`artifacts/layout-triage-20260619-000826-px68k-pico-20s/`。PICO 側の性能と gameplay は追加確認余地あり。 |
| TIC-80 | `retroarch:tic80`, `picoarch:tic80` | PicoArch 側は `RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK` 未対応で content load 前に失敗していたため、PicoArch に frame-time callback dispatch を追加。さらに TIC-80 core は `BUILD_STATIC=OFF` だと `script.c` の Lua backend が `Scripts[]` に登録されず、初回 `tic_init_vm()` へ進んだところで実行 backend が空になるため、TIC-80 CMake build に `-DBUILD_STATIC=ON` を追加した。2026-06-19 direct smoke で RA は `--max-frames 120` 正常終了、PICO は連続 `VIDEO_REFRESH` を確認し `pass_init`。 |
| ChaiLove | `retroarch:chailove`, `picoarch:chailove` | ChaiLove core は内部で SDL1 を使い、`SDL_VIDEODRIVER` が未指定または `fbcon` のままだと同梱 SDL の libretro video driver が選ばれず `Unable to initialize SDL No available video device` で失敗した。RA/PICO launcher で `chailove` のときだけ `SDL_VIDEODRIVER=LIBRETROvideo` を export するようにし、RA は 120 frames 正常終了、PICO は content load、`Screen: 480x480`、`Frame rate: 60` まで確認して `pass_init`。 |
| Fairchild Channel F | `retroarch:freechaf`, `picoarch:freechaf` | 実機に FreeChaF の必須 BIOS `sl31254.bin` と、`sl31253.bin` または `sl90025.bin` が無かった。core は実験的 HLE に fallback するが、検証 ROM `tents_CF.bin` では `Unsupported HLE function: 0xd0` で停止する。RA ではその後 dummy core に残るため、RA/PICO launcher で BIOS preflight を追加し、足りない場合は必要ファイル名と MD5 を出して `66` で即終了する。BIOS 入手後に再検証するため `untested` とする。 |
| Wolfenstein 3D | `retroarch:ecwolf`, `picoarch:ecwolf` | `ecwolf.pk3` 生成後も 320x200/RGB565 出力で stripe corruption が出ていた。`ecwolf-palette=xrgb8888` に切り替えると RA/PICO とも表示崩れが消えたため、RA/PICO launcher で ECWolf の既定 palette を XRGB8888 に seed する。2026-06-19 direct capture で RA title と PICO startup/credits を `pass_init`。 |
| ZX-81 | `retroarch:81`, `picoarch:81` | EightyOne core は SELECT で仮想キーボードを開くが、plumOS RetroArch の SELECT hotkey enable と衝突していた。RA launcher は `81` core のときだけ hotkey enable を A30 Function/R3 button へ移し、SELECT を core へ返す。PICO は Function を menu、SELECT を core SELECT に bind 済み。PICO の一見崩れた画面は `81_fast_load=disabled` による実時間 tape loading 表示だったため、81 default seed は fast load enabled にした。2026-06-19 direct capture で RA/PICO とも `blocky.p` の game screen まで到達し `pass_init`。 |

## 優先度 P1: system 全体が使えない、または代替が弱い問題

| group | affected profiles | first hypothesis | first action |
| --- | --- | --- | --- |
| なし | - | - | - |

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
