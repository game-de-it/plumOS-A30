# plumOS emulator build targets

この文書は、Miyoo A30 stockOS の `/mnt/SDCARD/Emu`、`/mnt/SDCARD/RApp`、
`/mnt/SDCARD/RetroArch/.retroarch/cores` を参考にしつつ、plumOS でビルドする
emulator / libretro core / standalone runtime を A30 の実用性で整理したものです。

stockOS のカテゴリや version を採用するための表ではありません。plumOS FE では stock
MainUI の「カテゴリ」は設計軸にせず、機種/ROM集合に対して SELECT で core/emulator
launch profile を選ぶ前提にします。

plumOS では RetroArch 本体、standalone emulator、補助 library は build 時点の
upstream latest stable を初期候補にします。libretro core は Onion が採用している core を
取り込み、Onion 側に実績 commit/build recipe がある場合はそれを優先します。Onion に無い
plumOS 独自採用 core は upstream latest/HEAD を採用候補にできます。A30 実機で動作異常、
性能退行、A30 固有不具合が出た場合は stockOS 側の version、patch、build option も
比較対象にします。

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
| libretro cores | `/mnt/SDCARD/plumos/retroarch/cores/*.so` | stockOS の core 名は参考のみ。現在の recipe は Onion 採用 core と plumOS 独自採用 core の union として、plumOS default の Class A/B 41 core と Onion catalog 補完用 Class O を `docker/plumos-toolchain/libretro-core-recipes.tsv` で管理する。A30 実機では `fceumm` と `gambatte` の画面表示、Onion 由来 commit の `mednafen_vb` 性能改善と Bad Apple 実動作を確認済みで、残りは system ごとの起動/performance/input/audio 検証が必要。 |
| PicoArch | `/mnt/SDCARD/plumos/emulators/picoarch/` | `shauninman/picoarch` commit `802047c` を plumOS 用 `platform=plumos` で build し、SDL1 は入力/音声だけに使い、ゲーム中は libretro core の RGB565 frame を A30 Mali/EGL presenter へ直接渡す。2026-06-15 に `fceumm` + `いっき` で実機表示の向き/左右反転、Native/Aspect/Full、Scanline/DMG/LCD、60fps 音声安定を確認済み。既存 `retroarch:<core>` に対応する検証用 `picoarch:<core>` 候補として Core Settings に出すが、初期 default にはしない。`quasi88` は 2026-06-16 の実機確認で RA と PICO のゲーム中 palette が一致せず、PICO 候補から除外する。 |
| standalone emulators | `/mnt/SDCARD/plumos/emulators/<id>/` | PPSSPP、ScummVM、EasyRPG Player、DOSBox Staging、PCSX-ReARMed の試作 build は `dist/plumos-standalone-emulators` に stage 済み。A30 実機検証後、PPSSPP/ScummVM/EasyRPG Player/PCSX-ReARMed は standalone profile 候補へ昇格し、DOSBox Staging は通常対象外にする。 |
| FFmpeg/FFPlay | `/mnt/SDCARD/plumos/apps/ffplay/` | stock の `Emu/ffplay` 相当。video player なので emulator 初期セットとは分ける。 |

注意: plumOS 同梱 SDL3+sdl2-compat は A30 実画面 video backend を持たないことを確認済み。
RetroArch minimal display probe は SDL を使わず、RetroArch の `mali_fbdev` context と
A30 rootfs の `/usr/lib/libEGL.so`/`libGLESv2.so` を使う。RGUI 表示は確認済みだが、
core を読み込んだ実ゲーム画面は `fceumm`/`gambatte` で確認済みです。現在の
`retroarch-minimal.cfg` は audio disabled のため、音声と emulator-facing input は次段階で検証します。

## Build status

2026-06-13 以降、libretro core は `docker/plumos-toolchain/libretro-core-recipes.tsv` を
正本にします。既定の `PLUMOS_CORE_FILTER=plumos` は Class A/B の 41 core を対象にし、
`PLUMOS_CORE_FILTER=onion` または `all` は Onion catalog 補完用 Class O も対象にします。
`scripts/inventory-onion-libretro-cores.sh` では、Onion 側にあって plumOS recipe に未登録の
core と、QuickNES のように plumOS recipe にだけある独自採用 core を確認します。
Onion core の `display_version`、prebuilt binary の更新 commit、builder recipe、plumOS
recipe の対応は [libretro core version inventory](libretro-core-version-inventory.md) と
`docs/libretro-core-version-inventory.tsv` に残します。

2026-06-07 の旧 bulk build では `./scripts/docker-build.sh libretro-cores` により Class A/B の
41 core を `dist/plumos-libretro-cores/plumos/retroarch/cores` に stage 済みで、
`manifest.txt` は実成果物ベースで `built=41`, `failed=0`, `skipped=0` でした。
この実績は historical baseline として残し、今後の再 build は Onion 採用 core と
plumOS 独自採用 core の union recipe を使います。

`vecx` は upstream HEAD の既定 build が OpenGL header を要求したため、A30 の fbdev/Mali/SDL
前提に合わせて `platform=armv HAS_GPU=0` で software path として build しました。
ScummVM は libretro Makefile の lite 構成、EasyRPG は最初の libretro build として
ICU/XML/一部音声補助を外した最小寄り構成です。これらは「build 済み」であり、実用性は
A30 実機の title/profile 別検証で判断します。

2026-06-13 の `PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 ./scripts/docker-build.sh libretro-cores`
では全 recipe を試行し、初回結果は `built=79`, `failed=18`, `skipped=0` でした。
この時点の長時間 build では一部 special core が `-j1` 固定だったため、以後の
`build-libretro-cores.sh` はまず `JOBS` の並列数で build し、失敗した同一条件だけ
`BUILD_JOB_FALLBACKS`、既定 `1`、で再試行します。古い libretro Makefile の一部は
ARM ELF 共有ライブラリを `*_libretro.dll` 名で出すため、staging では ELF と確認できる
`.dll` も `*_libretro.so` に正規化して配置します。

`PLUMOS_CORE_FILTER=all` など複数 core を対象にする build は、core 単位でも並列実行します。
既定は `LIBRETRO_CORE_BUILD_CONCURRENCY=4` で 4 core 同時処理です。`JOBS` は各 core 内で
`make`/`cmake --build` に渡す並列数で、未指定時は container CPU 数を core 並列数で割った値に
します。各 core の成果物は一時 staging へ分離して build し、最後に recipe 順で manifest と
成果物を merge するため、並列 build 中に manifest や output directory を同時書き込みしません。
ただし、過去の build で `-j1` 以外では失敗することが分かった core は
`LIBRETRO_SERIAL_CORES` であらかじめ `jobs=1` にします。既定の serial core は
`nestopia quicknes gambatte gpsp picodrive mednafen_pce_fast mednafen_supergrafx mednafen_ngp mednafen_lynx handy prosystem gw pokemini mednafen_vb dinothawr mrboom tgbdual`
です。これは build 条件を緩めるものではなく、同じ最適化条件のまま無駄な初回失敗を省くための
per-core job override です。

platform fallback で成功数を増やした core は、A30 向け最適化を保証できないため配布物として
扱いません。`build-libretro-cores.sh` は recipe の `make_args` だけを正とし、失敗時に
`platform=unix` などへ条件を緩める fallback を行いません。同一条件で `JOBS` だけを落とす
retry は、成果物の最適化条件を変えないため継続します。

2026-06-13 の後続調査では、残った 14 core を原因別に潰しました。`tic80` と
`dosbox_pure` は upstream の既定 branch が `main` へ移っていたため recipe ref を更新しました。
`dinothawr`、`freeintv`、`mrboom`、`tgbdual` は `classic_armv7_a7`、
`ecwolf`、`frodo`、`nekop2`、`uzem` は `unix-armv7-hardfloat-neon`、
`x1` は `crosspi` にして、各 Makefile の ARM 最適化分岐へ明示的に入れています。
`lutro` は `platform=armv7-hardfloat-neon` に加え、Lua の `AR=ar rcu` が command-line
`AR` override と衝突しないよう最小 patch を当てます。`squirreljme` は upstream の
libretro frontend が `ratufacoat` から `nanocoat` CMake target に移っていたため special
builder にしました。`mgba` は current HEAD の最小 libretro CMake build で scripting/docgen/LTO
を無効にし、生成 `version.c` と POSIX `memory.c` の重複を source list から外します。
この 14 core は
`PLUMOS_CORE_FILTER=mgba,tic80,dosbox_pure,dinothawr,ecwolf,freeintv,frodo,lutro,mrboom,nekop2,squirreljme,tgbdual,uzem,x1 FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores`
で `built=14`, `failed=0`, `skipped=83` を確認しました。同じ変更後の既定
`PLUMOS_CORE_FILTER=plumos` も `built=41`, `failed=0`, `skipped=56` です。
さらに serial core の初期 job override を入れた後、
`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores`
は全 97 recipe で `built=97`, `failed=0`, `skipped=0` になりました。この build で生成された
全 core の一覧は `docs/libretro-built-cores.tsv`、`systems.json` 反映用の system/core/拡張子/ROM
directory 候補は `docs/libretro-system-core-map.tsv` にまとめています。

調査した Makefile では `platform=armv` 分岐が `CC = gcc` のように compiler を host
gcc へ戻し、Docker の cross compiler 指定を潰す例がありました。そのため既定で
`LIBRETRO_FORCE_MAKE_TOOLCHAIN=1` とし、`make` の command-line 変数として
`CC/CXX/AR/RANLIB` を渡します。共通 flags は既定 `LIBRETRO_OPTIMIZATION_PROFILE=speed` とし、
`-O3` と Cortex-A7/NEON hard-float を使います。さらに攻める場合は
`LIBRETRO_OPTIMIZATION_PROFILE=aggressive` または `LIBRETRO_ENABLE_LTO=1` を明示します。
Makefile 側が後段で `-O2` などを追加して共通 flags を上書きする core は、recipe または
最小 patch で個別に直す対象です。既定では `LIBRETRO_PATCH_MAKEFILE_OPT_FLAGS=1` により、
Makefile 内の release 用 `-O* -DNDEBUG` を選択中の最適化プロファイルに揃えます。
`LIBRETRO_OPTIMIZATION_PROFILE=speed` では通常 `-O3`、`platform=classic_*` の速度優先分岐では
`-Ofast` を使い、`LIBRETRO_MAKE_OPT_FLAGS` で明示上書きできます。

libretro core の最適化検証は、`PLUMOS_CORE_FILTER=all` の一括成功数では判断しません。
`PLUMOS_CORE_FILTER=<core>` で 1 core ずつ build し、必要に応じて
`LIBRETRO_MAKE_ARGS_OVERRIDE='platform=...'` で候補 platform/build option を比較します。
候補が build できたら build log の compiler、最終 `-O`、Cortex-A7/NEON/hard-float、
dynarec/GPU 系 define を確認し、A30 実測で最も良い条件だけを
`libretro-core-recipes.tsv` に反映します。未検証 core を generic fallback で生成するより、
失敗として残して個別に潰すことを優先します。
Makefile optimization patch は、top-level Makefile だけでなく、core 配下の
`Makefile*`、`*.mk`、`*.mak` を浅い階層で確認します。これは `gambatte` のように
wrapper `Makefile` が `Makefile.libretro` を include し、include 先の `-O2` が
plumOS の `-O3`/`-Ofast` より後ろに追加されるケースを防ぐためです。

PicoArch は RetroArch の代替 frontend として扱い、core build recipe には含めません。
`./scripts/docker-build.sh picoarch` は picoarch 本体だけを build し、core は
`/mnt/SDCARD/plumos/retroarch/cores` の既存 libretro core を参照します。picoarch 本体は
plumOS glibc 2.36 で動かしますが、SDL1 は A30 stockOS の fbcon 実装を使います。Docker 由来の
汎用 `libSDL-1.2.so.0` を先に読ませると `VT_WAITACTIVE` で止まるため、picoarch package は
専用 libdir に glibc/libpng/zlib だけを置き、launcher の library path を
`picoarch/lib -> stockOS /usr/lib,/lib,/mnt/SDCARD/miyoo/lib -> plumOS lib` の順にします。
2026-06-15 には PicoArch 側の patch 済み `fceumm` も
`Makefile.libretro platform=miyoomini` で build して検証しました。生成 core は
`fceumm` commit `3f23e2b98f883be9c62a3fdb65c015d376dcd135`、
SHA-256 `5fb03352668ac34a86ce341caffe011c728d7f4a03f4d17a4b417d59ef8d591f` で、
`/mnt/SDCARD/plumos/emulators/picoarch/cores/fceumm_libretro.so` を full path 指定して
起動できます。しかし `/dev/fb0` capture は正常な `いっき` title 画面でも、実機 LCD は
灰色に崩れた表示のままでした。
したがって原因は libretro core ではなく、stock SDL1 の `640x480` fbcon mode set と
A30 LCD scanout の不整合です。CPU 回転での補正も性能面で成立しませんでした。

このため 2026-06-15 に PicoArch 用の `plat_a30_mali.c` を追加しました。menu などは
従来通り software surface へ 640x480 RGB565 で描画しますが、ゲーム中は PicoArch の
CPU scaler を通さず、libretro core から渡された RGB565 frame を直接 A30 Mali/EGL presenter へ
渡します。dlopen した stockOS の `/usr/lib/libEGL.so` と `/usr/lib/libGLESv2.so` で raw panel
EGL surface を作り、RGB565 texture として表示します。SDL1 は引き続き audio/input 初期化に
使いますが、表示の mode set には使いません。
既定は `PLUMOS_PICOARCH_A30_MALI=1`、`PLUMOS_PICOARCH_A30_ROTATION=ccw`、
`PLUMOS_PICOARCH_A30_VSYNC=1`、`PLUMOS_PICOARCH_A30_LINEAR=0` です。
presenter 初期化失敗時は通常失敗扱いにし、明示的に
`PLUMOS_PICOARCH_A30_FALLBACK_SDL=1` を指定した場合だけ stock SDL video fallback を許可します。
高速 direct present では PicoArch の `Screen size` の `Native`、`Aspect`、`Full` を
GPU 側の矩形で処理します。`Screen effect` は `None`、`Scanline`、`DMG`、`LCD` を
GPU 側で処理します。`DMG` は白寄せの液晶ブレンド、`LCD` は RGB サブピクセル風の
周期パターンとして shader で近似し、PicoArch の software scaler へ戻らないようにします。
古い/ユーザー編集済み `picoarch.cfg` で Gamepad 側の Function menu bind が欠落していても、
起動時に全 SDL input device へ既定の menu bind を再付与します。`EACTION_MENU` は
PicoArch の config 保存 action table にも追加し、既存 config に `bind escape = menu` /
`bind \xAA = menu` が無い場合は起動時に補完して保存します。

`fceumm_libretro.so` と `/mnt/SDCARD/Roms/FC/いっき.zip` の手動起動では、ログに
`picoarch-a30: mali presenter logical=640x480 physical=480x640 rotation=2 vsync=1` が出て、
`/dev/fb0` owner が PicoArch 1 process の状態で実機表示の向きと左右反転が解消しました。
2026-06-15 の追加確認では、従来の software 640x480 scaling 経路で約 1 core 相当を使い
音切れしていた `fceumm` + `いっき` が、direct present では `Native`/`None` で 59.9fps、
`Scanline`、`DMG`、`LCD` でも 60.0fps 付近を維持し、継続 underrun は出ませんでした。
確認 capture と log は `artifacts/a30-probes/` 配下の `picoarch-native-none-*`、
`picoarch-aspect-*`、`picoarch-scanline-*`、`picoarch-full-*`、
`picoarch-dmg-*`、`picoarch-lcd-*` にあります。
Core Settings では、`systems.json` にある既存 `retroarch:<core>` 候補から対応する
`picoarch:<core>` 候補を自動追加します。これは複数 system/core の動作確認用であり、
2026-06-15 時点では各 system の初期 default は既存の `default_launch_profile` のままにします。
その後の追加検証で、PicoArch の libretro environment を補完し、
`RETRO_ENVIRONMENT_SET_CORE_OPTIONS` の配列解釈と directory content の渡し方を修正しました。
これにより `gearboy`、`gearsystem`、`mednafen_lynx`、`dosbox_pure` などの起動時 crash と、
`easyrpg`、`scummvm`、`quasi88` の directory/no-game 系 content 読み込み不備を潰しています。
ログは `pa_log()` ごとに flush するようにし、TERM/KILL で終了した probe でも最後の初期化状況を
追いやすくしました。`tgbdual` は state/config を初期化しても実機 raw framebuffer が黒画面のため
除外します。PC Engine 系の `mednafen_pce` は RA/PICO とも実動作負荷が高いため通常候補から外し、
`mednafen_pce_fast` を使います。GB/GBC は PicoArch 検証では `gambatte`、`gearboy`、`mgba`、`vbam` を使います。
WonderSwan の `mednafen_wswan` は、`RETRO_ENVIRONMENT_SET_ROTATION` が true を返すと core が
frontend 側の hardware rotation を期待し、core 内の software rotation を使わなくなります。
PicoArch A30 presenter は A30 panel への最終回転だけを実装しており、libretro の display rotation は
未実装のため、`SET_ROTATION` は false を返して `mednafen_wswan` 側の core 内回転へ戻します。
`quasi88` は D88 の header seek/read が PicoArch の簡易 VFS 経由で失敗し、
`Image not found` の内蔵メニューへ落ちるため、`RETRO_ENVIRONMENT_GET_VFS_INTERFACE` だけ
false を返して core 内蔵の標準 file I/O に戻します。2026-06-16 の
`XeGrader100001.d88` 比較では RA/PICO とも同じ title 画面まで到達し、色差は見られませんでした。
同日の arcade 追加検証では、PicoArch の VFS が `vfsonly://` path と FBNeo の minizip
`fseek` 互換を満たしておらず、RA では起動する `gunforc2.zip` が PICO では
`No romset found` になることを確認しました。A30 patch で VFS path 正規化、任意の
`PLUMOS_PICOARCH_VFS_TRACE=1` trace、FBNeo 向け seek return 互換を追加し、
`fbneo`、`mame2003_plus`、`fbalpha2012`、`mame2000` の arcade ROM 初期化まで確認済みです。
なお RA/PICO どちらでも `mame2000` の Irem M92 系 driver には core 側で
`GAME_NO_SOUND` 指定の title があり、その場合の無音は plumOS audio 経路の不具合ではありません。
同日の PS1 追加検証では、PICO `pcsx_rearmed` が BIOS 検出後に `cdrom read failed for lba 4` で
CD image を無効扱いしていました。原因は `pcsx_rearmed` の libretro-common stdio transform が
VFS `seek` を `fseeko()` として扱い、成功時 `0` を期待する一方で、PicoArch 側は libretro VFS 仕様寄りに
現在位置を返していたことです。`pcsx_rearmed` も FBNeo と同じ C `fseek` 互換の戻り値にし、
短時間 probe では `chroQW.cue` が `Screen: 256x240` まで到達しました。
また launcher は `picoarch` 子プロセスを background 起動して PID を保持し、TERM/HUP/INT/EXIT の
cleanup で `picoarch` 本体と `plumos-joystickd` を両方止めます。これは probe や FE 終了時に
複数の PicoArch が残り、CPU/audio/fb0 owner 判定を汚す問題を避けるためです。

BIOS/system directory は PicoArch config の `bios_dir = /path` で指定できます。
未指定時は ROM directory 名を tag として `/mnt/SDCARD/Bios/<tag>` を返します。
`/mnt/SDCARD/Roms/FC/...` から `FC` ではなく `C` を tag として取ってしまう off-by-one を
2026-06-15 に修正しました。plumOS launcher の
`/mnt/SDCARD/plumos/config/standalone/picoarch.env` では
`PLUMOS_PICOARCH_BIOS_DIR=/path` を指定でき、これは `picoarch.cfg` が存在しない場合の
既定 system directory として扱います。core/ROM directory ごとの
`/mnt/SDCARD/plumos/state/picoarch/.picoarch-<core>-<tag>/picoarch.cfg` に
`bios_dir` がある場合は、そちらが最終的に優先されます。
BIOS pack の多くは Onion/Miyoo 互換の共有 BIOS root `/mnt/SDCARD/Bios` に置かれます。
そのため PicoArch launcher は、未指定時に下記 core の既定 system directory を共有 BIOS root
へ向けます。core/ROM directory 別の `picoarch.cfg` に `bios_dir` がある場合はそちらが優先です。

| group | core | BIOS path policy |
| --- | --- | --- |
| FDS | `fceumm`, `nestopia` | `disksys.rom` を `/mnt/SDCARD/Bios` から読む。 |
| PC Engine CD / SuperGrafx | `mednafen_pce_fast`, `mednafen_supergrafx`, `mednafen_pce` | `syscard*.pce` / `games_express.pce` を `/mnt/SDCARD/Bios` から読む。 |
| Sega CD / 32X | `genesis_plus_gx`, `picodrive` | `bios_CD_*.bin` / `32X_*_BIOS.BIN` を `/mnt/SDCARD/Bios` から読む。 |
| CD / 3D console | `pcsx_rearmed`, `mednafen_pcfx`, `opera`, `neocd`, `flycast`, `yabasanshiro`, `beetle_saturn`, `mupen64plus_next`, `parallel_n64`, `virtualjaguar` | PS1 / PC-FX / 3DO / Neo Geo CD / Dreamcast / NAOMI / Saturn / 64DD などの BIOS を `/mnt/SDCARD/Bios` から読む。 |
| handheld / 8-bit | `gpsp`, `mgba`, `mednafen_gba`, `meteor`, `vba_next`, `vbam`, `gambatte`, `gearboy`, `gearsystem`, `mednafen_lynx`, `handy`, `mednafen_ngp`, `mednafen_wswan`, `pokemini`, `freechaf`, `atari800`, `prosystem`, `freeintv`, `o2em` | optional/required BIOS を `/mnt/SDCARD/Bios` から読む。 |
| arcade BIOS pack | `fbneo`, `fbalpha2012`, `fbalpha2012_cps1`, `fbalpha2012_cps2`, `fbalpha2012_neogeo`, `mame2000`, `mame2003_plus` | `fbneo/neogeo.zip` などの arcade BIOS/data pack を `/mnt/SDCARD/Bios` 基準で探す。 |
| computer / data pack | `bluemsx`, `fmsx`, `puae`, `np2kai`, `nekop2`, `px68k`, `hatari`, `cap32`, `x1`, `bk`, `mu`, `vice_x64`, `vice_xvic`, `fuse`, `squirreljme`, `ecwolf`, `dosbox_pure`, `prboom` | system ROM / machine database / runtime data を `/mnt/SDCARD/Bios` 基準で探す。 |
| SNES special carts | `snes9x` | BS-X / Sufami Turbo BIOS を `/mnt/SDCARD/Bios` から読む。 |
| PC-88 | `quasi88` | 例外として `/mnt/SDCARD/Bios/quasi88` を使う。 |

2026-06-13 の first probe では、`scripts/probe-libretro-core-options.sh` で
`nestopia`、`quicknes`、`snes9x2005`、`mednafen_pce_fast`、
`mednafen_supergrafx` を比較しました。`nestopia`、`quicknes`、
`mednafen_pce_fast`、`mednafen_supergrafx` は `platform=classic_armv7_a7` が
実際に `-Ofast`/LTO へ入り、`BUILD_JOB_FALLBACKS=1` の `-j1` retry で生成できました。
`snes9x2005` は Makefile の分岐順により `classic_armv7_a7` が本来の classic 分岐へ
到達しないため、`platform=armv7-hardfloat-neon HAVE_NEON=1 ARCH=arm BUILTIN_GPU=neon`
を採用します。probe log は
`artifacts/libretro-core-option-probes/20260613-071812-nestopia`、
`20260613-072334-quicknes`、`20260613-072436-snes9x2005`、
`20260613-072521-mednafen_pce_fast`、
`20260613-072713-mednafen_supergrafx` にあります。

同日の second probe では `gambatte`、`gpsp`、`genesis_plus_gx`、`picodrive` を
比較しました。`gambatte`、`gpsp`、`picodrive` は `platform=classic_armv7_a7` で
`-Ofast`/LTO へ入り、`-j1` retry で生成できます。`genesis_plus_gx` は LTO には
入りませんが、`classic_armv7_a7` で `-Ofast` へ入り、host compiler 混入と残留
`-O2` はありません。`picodrive` は `platform/common/common.mak` の Cyclone tool
build に単独 `-O2` が残っていたため、`*.mak` も patch 対象に含めました。probe log は
`artifacts/libretro-core-option-probes/20260613-075643-gambatte`、
`20260613-074053-gpsp`、`20260613-074216-genesis_plus_gx`、
`20260613-080709-picodrive` にあります。

third probe では `mednafen_ngp`、`mednafen_wswan`、`mednafen_lynx`、`handy` を
比較しました。4 core とも `platform=classic_armv7_a7` を採用します。
`mednafen_ngp` は strict 条件で recipe の `platform=armv` が失敗し、classic の
`-Ofast`/LTO + `-j1` retry でのみ生成できました。`mednafen_lynx` と `handy` も
classic で `-Ofast`/LTO へ入り、`mednafen_wswan` は LTO なしですが `-Ofast` へ
入ります。probe log は
`artifacts/libretro-core-option-probes/20260613-081612-mednafen_ngp`、
`20260613-081750-mednafen_wswan`、`20260613-081818-mednafen_lynx`、
`20260613-081907-handy` にあります。

fourth probe では `stella2014`、`prosystem`、`potator`、`o2em`、`gw`、
`pokemini` を比較しました。6 core とも `platform=classic_armv7_a7` を採用します。
`prosystem`、`potator`、`gw`、`pokemini` は `-Ofast`/LTO へ入り、`stella2014` と
`o2em` は LTO なしですが `-Ofast` へ入ります。host compiler 混入と残留 `-O2` は
ありません。probe log は
`artifacts/libretro-core-option-probes/20260613-082357-stella2014`、
`20260613-082522-prosystem`、`20260613-082606-potator`、
`20260613-082629-o2em`、`20260613-082704-gw`、
`20260613-082811-pokemini` にあります。

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
| Red Viper | Virtual Boy 用 experimental probe | A30 で ARM dynarec は有効。StockOS 由来 SDL2 `mali` + GLES2 経路は `red-viper-sdlgl-a30` として build でき、画面向き/fit と単眼描画は修正済み。ただし Bad Apple の高負荷場面では音切れが残り、深い SDL audio queue でも改善しなかったため、通常配布と FE profile から外します。必要な場合だけ `PLUMOS_STANDALONE_FILTER=red_viper` で明示ビルドします。 |

## Virtual Boy note

Virtual Boy の `mednafen_vb`/Beetle VB は、2026-06-12 時点の旧 plumOS core では A30 で大きく
処理落ちしていました。当時の実機測定では、1344MHz/4core でも FBO 表示ありで約23fps、
null video/audio でも約25fpsでした。core option の `vb_cpu_emulation=fast` は既に有効で、
3D 表示 mode の変更や音声停止だけでは 50fps に届きませんでした。

ただし、2026-06-13 に Miyoo Mini Plus / Onion 4.3.1 の
`mednafen_vb_libretro.so` を A30 に一時配置して同じ null video/audio 条件で測定したところ、
Bad Apple 600 frames が 6.24秒、約96fps 相当で完走しました。比較のため、plumOS build の
Beetle VB core は同条件で 19.60秒、約30.6fps、Mini Plus 実機上の Onion core は 9.14秒、
約65.6fps でした。したがって、Beetle VB の問題は A30 SoC の単純な性能不足ではなく、
plumOS 側の Beetle VB core build / toolchain / upstream commit 差が主因である可能性が高いです。
Onion 側の Virtual Boy package は `lr-mednafen-vb` を使い、既定 core option も
`vb_cpu_emulation=fast` です。Onion の RetroArch は Miyoo Mini/Mini+ 向け Dingux build で、
CPU clock override 用の `cpuclock.txt` もサポートします。Mini Plus の公式推奨 OC は
1800MHz、最大目安は1900MHzですが、今回の A30 上の Onion core 測定は 1344MHz で十分高速です。
Onion repository には prebuilt core が
`static/build/RetroArch/.retroarch/cores/mednafen_vb_libretro.so` として含まれており、Mini Plus
実機から取得した core と md5 `cc34723254f0dfc17bce1f2f51a7bbfe` / BuildID
`e510367247958d87064b9def3649b30d50ac80c5` が一致しました。Package Manager は
`/mnt/SDCARD/App/PackageManager/data` 配下の package directory を `/mnt/SDCARD` へコピーする構造で、
VB package 自体は `Emu/VB` launcher/config と `Roms/VB` directory を追加し、core 本体は共通
RetroArch build 領域の既成バイナリを参照します。
Onion 本体 repository の CI は release/package を組み立てますが、少なくともこの core は
repository 内に commit 済みの prebuilt binary として扱われており、Onion 本体側で Beetle VB を
source から毎回 build している形ではありません。

Onion 側 core の由来は PR #624 / commit `bffa64e2c78505444b8dfc4ff0d0439b866b0d79`
で、commit message には `beetle-vb-libretro` を `v1.27.1 (aa77198)` から
`v1.31.0 (162918f)` へ更新した、と明記されています。binary 内の version string も
`v1.31.0 162918f` で、compiler string は `GCC: (Debian 8.3.0-2) 8.3.0` でした。
公式 `libretro/beetle-vb-libretro` の `162918f06d9a705330b2ba128e0d3b65fd1a1bcc` は
2022-08-28 の commit です。plumOS 現行 build は upstream `1275bd7` / GCC 12.2.0 なので、
Onion core とは source commit と toolchain の両方が異なります。

外部 builder としては `schmurtzm/Miyoo-Mini-Retroarch-builder` が一次候補です。2022-11-21 の
Onion PR 直前の builder commit `8a552f2` では、GitHub Actions が
`techdevangelist/miyoomini-buildroot:latest` Docker image を使い、`libretro-super` に対して
`./libretro-buildbot-recipe.sh ../cores-onionos` と `../cores-onionos-special` を実行します。
`cores-onionos` の `mednafen_vb` 行は `https://github.com/libretro/beetle-vb-libretro.git`
の `master` を `GENERIC Makefile .` で build する定義です。`cores-onionos.conf` は
`CC=arm-linux-gnueabihf-gcc`, `CXX=arm-linux-gnueabihf-g++`,
`STRIP=arm-linux-gnueabihf-strip`, `ARM_NEON=1`, `CORTEX_A7=1`, `ARM_HARDFLOAT=1` を設定し、
成果物は `libretro-super/dist/unix/*` として upload されます。

ただし当時の `libretro-super` submodule `b941477` では、recipe の `platform classic_armv7_a7` は
`libretro-config.sh` 側で専用 target として認識されず、`FORMAT_COMPILER_TARGET=unix` に落ちます。
したがって残っている recipe から推定できる実際の core build は、
`platform=miyoomini` や Beetle VB Makefile の `classic_armv7_a7` section 直指定ではなく、
cross compiler を環境変数で指定した `make -f Makefile platform=unix -j...` に近い可能性があります。
Actions artifact/log は保存期限切れのため、同一 hash の完全再現には builder image か GCC 8.3
相当で再 build して確認する必要があります。

2026-06-13 の追加測定では、plumOS toolchain / GCC 12.2.0 でも Beetle VB を upstream
`162918f06d9a705330b2ba128e0d3b65fd1a1bcc` に pin すれば Onion 相当の速度が出ることを確認しました。
同じ A30、1344MHz/4core、RetroArch null video/audio、Bad Apple 600 frames 条件で、現行
plumOS core `1275bd7` は 18.84秒 / 約31.85fps、Onion core は 5.73秒 / 約104.71fps、
plumOS build の `162918f` + `platform=armv` は 5.58秒 / 約107.53fps でした。
`162918f` + `platform=classic_armv7_a7` は 5.38秒 / 約111.52fps でしたが、GCC 12 LTO のため
`-j1` が必要になり、通常 build の安定性とのトレードオフがあります。現行 `1275bd7` は
`classic_armv7_a7` にしても 18.46秒 / 約32.50fps だったため、性能低下の主因は build option
ではなく upstream commit 差です。plumOS の通常 build recipe は、VB core を `162918f` に固定し、
速度優先で `platform=classic_armv7_a7` を使います。この条件は GCC 12 LTO のため `JOBS=4`
では link に失敗することがあり、その場合は同じ build 条件のまま `BUILD_JOB_FALLBACKS=1` で
`-j1` retry して生成します。
測定ログは `artifacts/a30-logs/mednafen-vb-core-compare-20260613.log` と
`artifacts/miyoo-mini-plus/ra-vb-null-600-scale1.*`、
`artifacts/vb-core-build-options-20260613/summary.txt` に残しています。

Red Viper は libretro core ではなく 3DS 由来の standalone emulator ですが、A30 の armv7
hard-float で ARM dynarec が動きます。zip から一時展開した raw `.vb` ROM を使った実験用
静的リンク probe の結果は以下です。

| condition | result |
| --- | --- |
| headless, no audio, no display, 1344MHz/4core, 1200 frames + 120 warmup | 322.86fps |
| software VIP render, no audio, 1344MHz/4core, 600 frames + 60 warmup | 289.00fps |
| software VIP render, no audio, 648MHz/2core, 600 frames + 60 warmup | 135.13fps |

現在の Virtual Boy 性能問題は、`mednafen_vb` を Onion 由来 commit に pin した libretro core で
解消できたため、Red Viper は通常配布と FE profile から外します。
`red-viper-a30` wrapper は Red Viper の software framebuffer を A30 の `/dev/fb0` へ
横向き表示し、A30 物理 input、Function in-game menu、START+SELECT 終了を扱います。FE は
`retroarch:mednafen_vb` だけを Virtual Boy の profile とします。A30 wrapper は 3DS 版 Red Viper の menu UI ではなく、
A30 用の軽量 menu と `plumos/config/standalone/red_viper.env` で設定を扱います。
単眼画面向けに `PLUMOS_A30_RED_VIPER_EYE=both` を既定にし、左右目を赤成分の最大値で合成します。
設定 inventory は [Red Viper A30 設定](red-viper-a30-settings.md) を正とします。
音声は上流 `vb_sound.c` と ALSA `default` 出力で有効化しています。A30 の実 PCM は
48kHz で開かれるため、Red Viper A30 build では `RED_VIPER_A30_AUDIO_RATE=48000`
を既定にし、ALSA latency 160ms、起動時 prebuffer、10ms chunk の producer queue を使います。
producer queue が空になった場合は直前の audio chunk を継続して、ROM 側の一時的な音声生成遅れで
ALSA buffer が空になることを避けます。上流既定の 50kHz のままでは音飛びと映像のカクつきが
出ました。`plug:default` による変換は A30 の libasound で assert するため使いません。
2026-06-13 の実機確認では、48kHz build でゲーム本編の音声/映像が正常に見えることを
確認しています。7z 展開は後続課題です。

2026-06-13 に Red Viper upstream の `source/linux-test` GLES2 frontend を使い、
stock SDL2 の `mali` video driver と rootfs `libGLESv2`/`libMali` へ接続する実験
binary `red-viper-sdlgl-a30` を直接起動しました。`badapple_mednafen.vb` では
`SDL_VIDEODRIVER=mali`、`gl_renderer=Mali-400 MP` となりました。15秒程度の軽い場面では
50fps 近くに見えることがありますが、120秒測定では平均 35.18fps、最小 14.49fps、
49fps 以上は 9.4% に留まりました。これは fbdev software presentation 版より改善する場面が
ある一方、Bad Apple のピーク負荷には届きません。
この経路の画面向き/fit は GLES 最終 quad の回転で修正済みです。2026-06-13 の
`ccw` capture では raw `480x640` が既存 FE と同じ向きになり、cw 変換後に横画面として
読めることを確認しました。さらに SDL/GLES frontend へ `PLUMOS_A30_RED_VIPER_EYE`、
`PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK`、SDL audio queue/prebuffer 設定を接続し、
`EYE=left` では `present_fps` 平均約49fpsまで改善しました。しかし高負荷場面では
42fps台まで落ち、音声途切れは深い audio queue でも目立つ改善がありませんでした。
このため Red Viper は通常配布では build せず、FE profile にも出しません。
Red Viper/Reality Boy 系の license は配布前に third-party notice と利用条件を確認します。

## Class A: 初期ビルド対象

A30で満足に動く可能性が高く、plumOSの初期対応に入れる候補。

| system / ROM family | launch profile candidates | stock discovery | note |
| --- | --- | --- | --- |
| FC / NES / FDS | `fceumm`, `nestopia`, optional `quicknes` | `Emu/FC`, `RApp/fceumm`, `RApp/nestopia`, plumOS `quicknes` | Onion 採用 core を優先しつつ、QuickNES は plumOS 独自の軽量候補として残す。 |
| GB / GBC | `gambatte`, optional `mgba` | `Emu/GB`, `RApp/gambatte`, backup `gambatte_color` | 低負荷。 |
| GBA | `gpsp`, `mgba`, optional `vba_next` | `Emu/GBA`, `RApp/gpsp`, `RApp/mgba`, backup `vba_next` | `gpsp` performance first、`mgba` accuracy fallback。 |
| Master System / Game Gear / SG-1000 | `genesis_plus_gx` | `Emu/MS`, `RApp/genesis_plus_gx_gg`, backup `genesis_plus_gx_ms/sg` | MD系profileと共通化できる。 |
| Mega Drive / Genesis | `genesis_plus_gx`, `picodrive` | `Emu/MD`, `RApp/genesis_plus_gx`, `RApp/picodrive` | 32X/Mega CDは別profile扱い。 |
| Mega CD / Sega CD | `genesis_plus_gx`, `picodrive` | `RApp/picodrive`, backup `picodrive_cd` | CD系だがA30でも現実的な候補。 |
| 32X | `picodrive` | `Emu/MD`, `RApp/picodrive` | title-dependent寄りだが、軽量なものは候補に残す。 |
| SFC / SNES | `snes9x`, `mednafen_supafaust`, optional `snes9x2005-plus` | `Emu/SFC`, `RApp/snes9x`, `RApp/snes9x2018` | enhancement chip title は個別検証。 |
| PC Engine / TurboGrafx-16 | `mednafen_pce_fast` | `Emu/PCE`, `RApp/mednafen_pce_fast` | 低負荷。 |
| PC Engine CD | `mednafen_pce_fast` | backup `mednafen_pce_cd` | PS1以外のCD系では有力。BIOS/CHD/CUE確認が必要。 |
| SuperGrafx | `mednafen_supergrafx` or `mednafen_pce_fast` | backup `mednafen_supergrafx` | PC Engine の海外名ではなく上位互換機。PC Engine content での確認は PC Engine profile の確認として扱い、SuperGrafx 専用 content は別途確認する。 |
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
| Virtual Boy | `retroarch:mednafen_vb` | `mednafen_vb` は Onion 由来 commit `162918f` へ pin した plumOS build で Bad Apple の実動作を確認済み。Red Viper は StockOS 由来描画経路の向き/fit と単眼描画は修正済みだが、高負荷場面の音切れが残るため FE profile から外す。 |
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

現行 recipe では Class A/B の 41 core を plumOS default として build します。次は deploy 後の
実機検証を以下の順で進めます。

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
