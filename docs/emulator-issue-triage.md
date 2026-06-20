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
| `pass_init` | 46 | 起動や初期表示は成立したが、gameplay/入力/音声/性能の追加確認余地あり |
| `retired` | 52 | 方針判断済みで通常 FE/動作確認対象から外した |
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
| mGBA | GB/GBA の `retroarch:mgba`, `picoarch:mgba` | mGBA core の CMake source list から `version.c` と `src/platform/posix/memory.c` を外していたため、`projectName` / `anonymousMemoryMap` が未解決になっていた。不要な除外をやめ、PicoArch の library path は plumOS lib を system lib より先に見る順序へ変更。2026-06-18 direct smoke で GB/GBA は `pass_init`。GBC は実機に `.gbc` ROM がなく `untested`。2026-06-20 に fresh SD 用 runtime package が古い `plumos-libretro-cores-onion-lock-all` の mGBA を取り込んで同じ `projectName` エラーへ戻っていたため、`dist/plumos-libretro-cores-mgba-fix` を runtime package の optional 後勝ち overlay にした。 |
| Commodore 64 | `retroarch:frodo`, `retroarch:vice_x64`, `picoarch:frodo`, `picoarch:vice_x64` | `vice_x64` は ROM なし起動と最小 `.prg` autostart で RA/PICO とも正常表示。`frodo` は最小 `.p00` で RA/PICO とも C64 BASIC 画面を正常表示。Pacmania.zip は中身が `.tap` で、`frodo` は TAP 非対応、`vice_x64` は `Datasette: Cannot read in tap-file` を出すため、画面破損ではなく検証 ROM 形式/内容の相性問題として `pass_init` に戻した。capture は `artifacts/c64-video-20260618-234134-vice-nogame/`、`artifacts/c64-video-20260618-234449-vice-prg/`、`artifacts/c64-video-20260618-234759-frodo-p00/`。 |
| X68000 | `retroarch:px68k`, `picoarch:px68k` | 実機の `/mnt/SDCARD/Bios/keropi` は `IPLROM.DAT` / `CGROM.DAT` のような大文字名で、PX68k core は小文字名だけを探していた。`px68k-libretro-uppercase-bios.patch` で大文字 BIOS 名も候補に入れ、RA/PICO とも `CH68_110.xdf` で Human68k boot 画面まで到達。capture は `artifacts/layout-triage-20260619-000721-px68k-ra-corepatch/`、`artifacts/layout-triage-20260619-000826-px68k-pico-20s/`。PICO 側の性能と gameplay は追加確認余地あり。 |
| TIC-80 | `retroarch:tic80`, `picoarch:tic80` | PicoArch 側は `RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK` 未対応で content load 前に失敗していたため、PicoArch に frame-time callback dispatch を追加。さらに TIC-80 core は `BUILD_STATIC=OFF` だと `script.c` の Lua backend が `Scripts[]` に登録されず、初回 `tic_init_vm()` へ進んだところで実行 backend が空になるため、TIC-80 CMake build に `-DBUILD_STATIC=ON` を追加した。2026-06-19 direct smoke で RA は `--max-frames 120` 正常終了、PICO は連続 `VIDEO_REFRESH` を確認し `pass_init`。 |
| ChaiLove | `retroarch:chailove`, `picoarch:chailove` | ChaiLove core は内部で SDL1 を使い、`SDL_VIDEODRIVER` が未指定または `fbcon` のままだと同梱 SDL の libretro video driver が選ばれず `Unable to initialize SDL No available video device` で失敗した。RA/PICO launcher で `chailove` のときだけ `SDL_VIDEODRIVER=LIBRETROvideo` を export するようにし、RA は 120 frames 正常終了、PICO は content load、`Screen: 480x480`、`Frame rate: 60` まで確認して `pass_init`。 |
| Fairchild Channel F | `retroarch:freechaf`, `picoarch:freechaf` | 実機に FreeChaF の必須 BIOS `sl31254.bin` と、`sl31253.bin` または `sl90025.bin` が無かった。core は実験的 HLE に fallback するが、検証 ROM `tents_CF.bin` では `Unsupported HLE function: 0xd0` で停止する。RA ではその後 dummy core に残るため、RA/PICO launcher で BIOS preflight を追加し、足りない場合は必要ファイル名と MD5 を出して `66` で即終了する。BIOS 入手後に再検証するため `untested` とする。 |
| Wolfenstein 3D | `retroarch:ecwolf`, `picoarch:ecwolf` | `ecwolf.pk3` 生成後も 320x200/RGB565 出力で stripe corruption が出ていた。`ecwolf-palette=xrgb8888` に切り替えると RA/PICO とも表示崩れが消えたため、RA/PICO launcher で ECWolf の既定 palette を XRGB8888 に seed する。2026-06-19 direct capture で RA title と PICO startup/credits を `pass_init`。 |
| ZX-81 | `retroarch:81`, `picoarch:81` | EightyOne core は SELECT で仮想キーボードを開くが、plumOS RetroArch の SELECT hotkey enable と衝突していた。RA launcher は `81` core のときだけ hotkey enable を A30 Function/R3 button へ移し、SELECT を core へ返す。PICO は Function を menu、SELECT を core SELECT に bind 済み。PICO の一見崩れた画面は `81_fast_load=disabled` による実時間 tape loading 表示だったため、81 default seed は fast load enabled にした。2026-06-19 direct capture で RA/PICO とも `blocky.p` の game screen まで到達し `pass_init`。 |
| PicoArch fceumm | NES/FDS の `picoarch:fceumm` | Onion-era の RA 用 `fceumm_libretro.so` は PicoArch では `retro_load_game` 周辺から戻らず、`Loading ...` で止まっていた。PicoArch 用に `Makefile.libretro platform=miyoomini` で build した `fceumm` commit `3f23e2b98f883be9c62a3fdb65c015d376dcd135` は NES/FDS とも `Screen: 256x224`、`Frame rate: 60.099827` まで到達するため、launcher は `picoarch:fceumm` だけ `/mnt/SDCARD/plumos/emulators/picoarch/cores/fceumm_libretro.so` を優先する。2026-06-19 direct smoke で `pass_init`。 |
| PicoArch frameskip duplicate frame | frameskip 有効時に `video_refresh(NULL, ...)` を返す core | A30 Mali presenter では `data == NULL` frame で何も描画せず `eglSwapBuffers` もしなかったため、frameskip 時の点滅や frame pacing 崩れにつながり得た。`data == NULL` のときは前回 upload 済み texture を再描画して swap するように変更。2026-06-19 direct trace で `mame2000` が `data=(nil)` を返すことを確認し、`kungfum.zip` capture で正常表示を確認。 |
| PicoArch mame2000 frameskip default | Arcade の `picoarch:mame2000` | `mame2000-frameskip=auto` は起動直後から `video_refresh(NULL, ...)` を返し、画面更新間引きと音声 underrun の切り分けを難しくしていた。PicoArch presenter は skipped frame を repeat/swap し、mame2000 の既定値は `disabled` に変更した。最終的には Arcade PICO 全体の frame pacing/音声安定性が RA に届かないため通常候補から外した。 |
| Arcade PicoArch performance inversion | Arcade の `picoarch:fbneo`, `picoarch:fbalpha2012`, `picoarch:mame2000` | PicoArch は通常 RetroArch と同等以上の軽さを期待できるが、Arcade core では A30 実機上で RetroArch のほうが安定した。推測として、RetroArch の成熟した video/audio sync、core option、buffer handling が Arcade core の複雑な audio/frame pacing に強く、PicoArch の新しい A30 Mali presenter/audio loop では texture upload、`eglSwapBuffers`、duplicate frame、audio underrun の小さな差が実用性能に出ている。RA 側は pass なので Arcade の PICO companion は通常候補から外した。 |
| PicoArch TyrQuake | `picoarch:tyrquake` | PicoArch が `retro_load_game()` 前に `retro_set_controller_port_device()` を呼ぶ一方、TyrQuake はその中で Quake console bind を実行するため、`Host_Init()` 前の command path で SIGSEGV していた。TyrQuake だけ controller setup を content load 後へ遅延し、2026-06-19 direct smoke で `139` が消え、`Screen: 320x200` と連続 `VIDEO_REFRESH`、gameplay framebuffer capture を確認して `pass_init`。 |
| PicoArch Lutro | `picoarch:lutro` | Lutro は `RETRO_ENVIRONMENT_GET_PERF_INTERFACE` を必須扱いにしており、PicoArch が未対応だったため content load 前に `Core needs the perf interface` で失敗していた。PicoArch に `get_time_usec`、counter、CPU feature、perf register/start/stop/log の最小実装を追加し、2026-06-19 direct smoke で `Screen: 320x240`、XRGB8888 video refresh、Pong gameplay capture を確認して `pass_init`。 |
| PicoArch rotated-axis input | `picoarch:hatari`, `picoarch:prboom`, `picoarch:dosbox_pure` | launcher の rotated-axis default で `axisYR` / `axisXR` を joystickd へ渡す。2026-06-19 direct launch で 3 core とも launcher log と `joystickd-last.log` の `x_source=axisYR y_source=axisXR` を確認し、runtime TSV は `pass_init` へ戻した。 |
| PicoArch EasyRPG boot classification | `picoarch:easyrpg` | 2026-06-19 direct RA/PICO 比較で、PICO も `Screen: 320x240`、`Frame rate: 60`、EasyRPG startup まで到達することを確認。起動不能ではないが、RA/PICO とも libretro route は MP3 BGM unsupported warning が残る。standalone が user-confirmed pass のため libretro route は通常候補から外した。 |
| SquirrelJME PICO-only classification | `picoarch:squirreljme` | `Cento.jar` は RA/PICO とも `SquirrelJME Init` 後に `JVM Exec Error: -7` を返す。PicoArch 共通層の P3 問題ではなく、core/content 互換性問題として扱う。RetroArch は user-confirmed pass のため、PicoArch companion は通常候補から外した。 |
| MAME2003+ / Neo Geo set split | Arcade/Neo Geo/MAME 2003+ の `mame2003_plus` | `ddragon.zip` は RA/PICO とも起動する一方、Neo Geo の `fatfury1.zip` は MAME2003+ で `sfix.sfx`、`mame.sm1`、`033-m1.bin`、`mamelo.lo` が不足し、ROM set mismatch と確定。Neo Geo では `fbneo` / `fbalpha2012` が pass のため `mame2003_plus` を通常 FE profile から外した。Arcade では RA route を残し、PicoArch companion は後続の performance inversion 整理で通常候補から外した。 |
| Neo Geo CD via FBNeo | `retroarch:fbneo`, `picoarch:fbneo` | FBNeo は 800x600 metadata までは到達するが、user-confirmed gameplay は `neocd` のみ。Neo Geo CD の default と通常 profile は `retroarch:neocd` に寄せ、FBNeo route は `retired`。 |
| PICO-8 default route | `retroarch:fake08`, `picoarch:fake08`, `retroarch:retro8`, `picoarch:retro8` | `retroarch:fake08` は `Celeste.p8` の bounded direct smoke で `pass_init`。ただし `picoarch:fake08` は既に user-confirmed `pass` のため、PICO-8 の default は `picoarch:fake08` に変更。`retro8` は BGM 異常が残るため通常候補から外した。 |
| Cave Story / NXEngine | `retroarch:nxengine`, `picoarch:nxengine` | `picoarch:nxengine` は direct smoke で 320x240/60fps metadata まで到達し、boot failure ではないと確認。ただし NXEngine README は Cave Story 1.0.0.6 + Aeon Genesis translation を要求しており、日本語版データの文字化けは RA/PICO 共通。現行 route は通常候補から外し、Cave Story system は disable にした。 |
| Lutro / LowRes NX / VMU video routes | `retroarch:lutro`, `picoarch:lowresnx`, `retroarch:vemulator`, `picoarch:vemulator` | Lutro RA は capture で中央線が表示され `pass_init`。LowRes NX PICO は `SET_PIXEL_FORMAT` が `retro_init()` 前に来る core で XRGB8888 が 0RGB1555 に戻る不具合を修正し、capture で正常表示。VMU は RA が unload 時 abort 134 のため `retired`、user-confirmed pass の `picoarch:vemulator` を default にした。 |
| P4 performance limit after HW display check | `opera`, `virtualjaguar`, `mednafen_pcfx`, `uw8`, `uzem`, GBA heavy cores, non-fast `mednafen_pce`, RetroArch `scummvm`, heavy `mame2003_plus` routes | RetroArch practical config と直近 log で `video_driver=gl`、`video_context_driver=mali_fbdev`、`fbdev_mali`、`Mali-400 MP` を確認。PicoArch logs でも P4 対象を含む各 core で `a30_mali=1` と `picoarch-a30: mali presenter ... vsync=1` を確認。HW 表示経路でも性能不足が残るため、代替のある system は重い profile を外し、代替のない system は disabled/retired にした。 |
| Non-performance fail cleanup | `retroarch:easyrpg`, `picoarch:easyrpg`, `picoarch:squirreljme`, `retroarch:nekop2`, `retroarch:retro8`, `picoarch:retro8`, `retroarch:nxengine`, `picoarch:nxengine` | `fail_perf` 以外で残っていた失敗 profile は、正常な代替 route があるものを通常候補から外し、Cave Story は現行 NXEngine data-language 条件が plumOS の通常対象に合わないため system disable とした。自動 PicoArch companion のブロックリストも同期済み。 |

## 優先度 P1: system 全体が使えない、または代替が弱い問題

| group | affected profiles | first hypothesis | first action |
| --- | --- | --- | --- |
| なし | - | - | - |

## 優先度 P2: 代替はあるが、直ると選択肢が増える問題

| group | affected profiles | reason |
| --- | --- | --- |
| なし | - | P2 は解決、または default/profile 方針へ反映済み。 |

## 優先度 P3: PicoArch 共通層の問題

| symptom | affected profiles | direction |
| --- | --- | --- |
| なし | - | PicoArch 共通層として扱っていた P3 は解決済み、または core/content/per-system 問題へ再分類済み。 |

## 優先度 P4: 性能限界として扱う候補

なし。P4 候補は HW 表示経路に乗っていることを確認済みで、性能不足として通常 FE/動作確認対象から外した。

## 次に実施する調査順

1. P2/P3/P4 と `fail_*` profile 整理は完了。残件は BIOS/合法 ROM 待ち、または user gameplay での `pass_init` 確認へ回す。
