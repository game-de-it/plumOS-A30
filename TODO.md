# TODO

## Phase 0 - Repository and Remote Access

- [x] `/Users/kroot/plumOS-A30` に local git repository を用意する。
- [x] SD カードから起動できる開発用 SSH kit を作成する。
- [x] 作業用 PC の公開鍵を `plumos/ssh/etc/authorized_keys` に置けるようにする。
- [x] A30 の Ports から `Start SSH` を実行して SSH 接続できるようにする。
- [x] `scripts/collect-a30-info.sh root@A30_IP_ADDRESS` で実機情報を収集する。
- [x] 起動フロー、mount、Wi-Fi、stock frontend、RetroArch 構成を文書化する。
- [x] 日本語を正本、英語を `.en.md` とするドキュメント構成にする。
- [ ] GitHub 公開前に remote、license、release 方針を確定する。

## Phase 1 - Docker Toolchain and Build Environment

- [x] plumOS 専用の Docker build environment を設計する。
- [x] `docker/` 配下に toolchain Dockerfile と補助 script を置く。
- [ ] A30 向け sysroot を Docker build で再現可能にする。
- [x] Docker 内で frontend、helper、RetroArch、libretro core、standalone emulator を build できるようにする。
- [ ] SDL2、RetroArch、libretro core、standalone emulator、補助 library は build 時点の latest stable upstream release を確認し、採用 version/tag/commit/build option を manifest 化する。
- [x] SDL2 互換 runtime は upstream SDL3 3.4.10 + sdl2-compat 2.32.68 を採用し、tag/URL/SHA-256/build option を manifest 化する。
- [x] Docker 内で RetroArch 1.22.2 minimal display probe を build できるようにする。
- [x] RetroArch minimal build は upstream tag/URL/SHA-256/build option を manifest 化する。
- [x] Docker 内で frontend compatibility scanner を build できるようにする。
- [x] Docker build output を `dist/` または staging directory へ集約する。
- [x] build cache と大きな生成物が git に入らないよう ignore/配置を整理する。
- [x] Docker image の作り方と使い方を日本語/英語で文書化する。

## Phase 2 - plumOS Runtime Layout

- [ ] `/mnt/SDCARD/plumos` 配下の runtime directory 構成を package 化する。
- [ ] `bin/`, `lib/`, `runtime/`, `frontend/`, `retroarch/`, `config/`, `state/`,
  `cache/`, `logs/`, `ssh/` の役割を固定する。
- [x] stock `/mnt/SDCARD/miyoo/lib` に依存しない `plumos-env` を作る。2026-06-08 に BusyBox/userland package で `/mnt/SDCARD/plumos/bin/plumos-env` を追加/deployし、clean PATH smoke で確認済み。
- [x] stock BusyBox の癖を避けるため、plumOS 用の静的 BusyBox を build/deploy する。
- [x] vfat で扱えるよう BusyBox applet を symlink ではなく wrapper script 化する。
- [ ] Debian に近い操作感のため `procps-ng`, `coreutils`, `util-linux` などを
  `plumos/gnu/bin` に追加する。2026-06-08 に first-pass として Debian armhf package の
  `iproute2` (`ip`, `ss`) と `strace` を `plumos/gnu/bin` wrapper +
  `plumos/gnu/libexec` 実体として追加し、A30へdeploy済み。
- [x] `ps`, `top`, `df`, `free`, `tar`, `find`, `grep`, `sed`, `awk`, `ip`, `ss`,
  `lsof`, `strace` の互換性を確認する。2026-06-08 に
  `scripts/probe-a30-userland-tools.sh` を追加し、A30実機で
  `PATH=/mnt/SDCARD/plumos/gnu/bin:/mnt/SDCARD/plumos/bin` の clean env smoke を実行した。
  最新 artifact は `artifacts/a30-probes/userland-tools/20260608-155401`。
  結果は `pass=11 usable_with_limits=6 missing=0 fail=0`。
  `ip`、`ss`、`strace` は `plumos/gnu/bin` から実行され、`strace` は
  `busybox true` の実traceまで確認済み。`ps`、`top`、`lsof`、`netstat` は
  BusyBox 版として usable with limits。
- [x] 動的 link が必要な場合の dynamic linker/shared library 同梱方針を SDL2 probe で検証する。
- [x] 実機上で plumOS runtime path だけを使う smoke test を行う。2026-06-08 の
  `scripts/probe-a30-userland-tools.sh` は BusyBox `env -i` + BusyBox `sh` で
  `plumos/gnu/bin:plumos/bin` だけを PATH にして実行済み。

## Phase 3 - Device Deployment Loop

- [x] Docker 内で build した成果物を A30 へ転送する deploy script を作る。
- [x] SSH/SCP/rsync 相当の転送方法を決める。
- [x] 転送後に実機で command を実行し、log を回収する helper を作る。
- [ ] build -> deploy -> run -> collect logs の一連の流れを 1 command にまとめる。
- [ ] SD カードだけで復旧できる rollback 手順を文書化する。

## Phase 4 - Boot Bootstrap

- [x] rollback 可能な `MainUI` wrapper を作る。
- [x] wrapper から `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` を通常FEとして起動できるようにする。
- [x] wrapper 起動失敗時に stock MainUI へ戻れる fallback を作る。
- [x] wrapper と plumOS frontend の log を `/mnt/SDCARD/plumos/logs` へ出す。
- [x] stock MainUI を残したまま plumOS frontend prototype を手動起動して検証する。
- [x] boot 時に stock `/etc/main` から起動される wrapper で stock `keymon` を止め、plumOS network rescue helper を自動実行する。
- [x] reboot 復旧用に controller UI の `--rescue-network` を追加し、A ボタンで Wi-Fi 起動処理、DHCP、SSH start を再実行できるようにする。
- [x] A30 の電源再投入後に network rescue で復旧可能な状態を保てることを確認する。2026-06-08 に boot wrapper が `plumos-network-rescue` を先に起動し、その後 `plumos-controller-ui-mali --rescue-network --rotation auto` を起動する経路で復旧を確認済み。2026-06-09 に boot 表示は通常FE TOPへ入る方針へ変更し、network recovery は START menu の `Network Recovery` 画面から A ボタンで実行する導線にした。左右キーは実行/戻るに使わない。
- [x] display probe が boot wrapper を stock fallback へ進ませないようにする。2026-06-08 に `scripts/probe-a30-safe-hotkeyd.sh` が `stop-a30-display-processes.sh` で wrapper 待受中の controller UI を止め、`/etc/main` -> `MainUI.stock` fallback が残る問題を確認した。probe 側で `/etc/main`、`/mnt/SDCARD/miyoo/app/MainUI`、`MainUI.stock` を停止し、`residual_boot_mainui` が残る場合は失敗扱いに変更済み。修正後の `artifacts/a30-probes/safe-shutdown/20260608-181818-text-ui-autohotkey-signal-nes` は `Result: ok`、完全一致プロセス確認では `plumos-controller-ui-mali.bin --rescue-network --rotation auto` だけが残り、capture `artifacts/a30-captures/after-hotkey-probe-wrapper-fix-20260608/20260608-181936.visible.cw.png` で NETWORK/WIFI 表示を確認済み。
- [ ] stock MainUI には reboot 項目がないため、`reboot` command が暗転/LED消灯後に Miyoo logo へ進まない問題を調査し、安全な OS 再起動可否を決める。

## Phase 5 - Frontend Compatibility Layer

- [x] `Emu`, `RApp`, `App`, `Themes` の stock `config.json` を読む。
- [x] stock `config.json` は plumOS 新仕様ではなく inventory/migration 入力だと明文化する。
- [x] `rompath`, `imgpath`, `gamelist`, `launchlist` を解決して存在と件数を確認する。
- [x] ROM file と artwork file の名前対応 rule を確認する。
- [ ] `gamelist` XML の stock data schema を読む。
- [ ] 既存 ROM、artwork、save、state を移動せずに認識できるようにする。
- [x] compatibility scanner で `extlist` による ROM count filter を実装する。
- [ ] frontend の ROM list で `extlist` による filter を実装する。
- [x] compatibility scanner で `launchlist` entry を検出する。
- [ ] `launchlist` の代替 launcher/core を UI と launch profile で扱えるようにする。
- [x] compatibility scanner で `Roms/recentlist.json` の存在と entry 数を検出する。
- [ ] `Roms/recentlist.json` 互換の recent list を parse/update できるようにする。
- [ ] `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path `$1` の launch 互換を再現する。
- [ ] 最終的には shell launch script 依存を plumOS launch profile へ移す。
- [x] stock 仕様を流用したくなった場合の decision record template を作る。

## Phase 6 - Frontend Implementation

- [x] plumOS frontend 独自の system/app/theme data model を設計する。
- [x] `systems.json` の初期 seed を作る。
- [x] Miyoo/ROCKNIX directory alias を使った ROM scan prototype を作る。
- [x] 機種選択時に対象 system だけ ROM directory を再 scan する prototype を作る。
- [x] `scan_directories=true` を追加し、ScummVM/EasyRPG のような directory 型 game を ROM entry として扱えるようにする。
- [x] 1000 dummy ROM files で text mode 初回表示時間を計測する。
- [x] SSH から確認できる text mode system list / ROM list prototype を作る。
- [x] START menu から settings/apps/network/shutdown へ辿る UI model を実装する。
- [x] TOP の system 選択中に SELECT で system default core を選ぶ UI を実装する。
- [x] ROM list の ROM 選択中に SELECT で per-ROM core override を選ぶ UI を実装する。
- [x] core 選択の優先順位を ROM override > system override > plumOS default > auto detect として実装する。
- [x] core 選択は core id だけでなく launch profile id として保存できるようにする。
- [x] SELECT core menu に CPU frequency preset を表示し、`core-overrides.json` で system/ROM 別に `performance|fixed` と fixed kHz を保存できるようにする。ユーザー向け preset から `keep` は削除済み。
- [x] 2コア/4コアのバッテリー消費差を dummy CPU負荷で測定し、差が大きかったため SELECT core menu に CPU core count preset を追加する。
- [x] ROM list で favorite toggle を実装し、START menu から Favorites を開けるようにする。
- [x] Favorites を TOP に仮想 system として表示する optional setting を用意する。
- [x] `recent.json` で履歴 ROM/launch profile/resume availability を管理する。
- [x] `resume-session.json` で自動 ROM 再開対象の ROM/core/launch profile/pending 状態を管理する。
- [x] `boot_resume_mode=off|last|picker` の text UI prototype を実装する。
- [x] 起動時に pending resume session があれば同じ ROM/core/launch profile の launch plan を作り、`boot --execute` で自動起動する。
- [x] controller-first の最小 frontend prototype を作る。
- [x] controller UI に system list、ROM list、recent、favorites、settings 画面を実装する。
- [x] theme、font、artwork の読み込み方針を決める。
- [x] ROM alias root からの相対 path を保持した thumbnail lookup を実装する。
- [x] thumbnail lookup で subdirectory 優先、flat 配置 fallback、placeholder fallback を実装する。
- [ ] ROM thumbnail scraping 方針を決める。推奨案は RetroArch の downloader を直接使わず、libretro-database 由来の CRC/serial -> canonical ROM name index と libretro-thumbnails 互換 path/index を plumOS 側で持つ。
- [ ] `plumos-artwork-scraper` を設計する。ROM scan cache の size/mtime/CRC を再利用し、system別DB index、thumbnail server index、negative cache、並列download、resume/retry を持たせる。
- [ ] Gallery mode 用に original thumbnail と A30表示用 resized/cache image を分け、初回表示は text/placeholder、画像取得は background queue で進める。
- [x] theme、layout preset、frontend behavior を分離した plumOS theme model を設計する。
- [x] `/mnt/SDCARD/plumos/themes/<theme_id>/theme.json` の schema を定義する。
- [x] theme は色、font、背景、system icon、selection 表現、spacing、thumbnail frame、sound effect だけを扱うようにする。
- [x] button 操作、START menu、SELECT core menu、favorite、ROM scan、resume などの behavior は theme から変更できないようにする。
- [x] text mode は theme asset/font が壊れていても default font/color で必ず操作できる fallback を実装する。
- [x] stock theme 互換は初期仕様に含めず、必要になった場合だけ importer として検討する。
- [x] brightness、volume、Wi-Fi、keymap など A30 設定 UI の扱いを決める。
- [ ] A30 settings の mixer/sysfs runtime backend 連動を検証する。System Settings の
  plumOS-owned 限定 write は backup、atomic write、`sync` 付きで実装済みだが、
  物理音量ボタン、brightness hotkey、即時 runtime 反映は未接続。
- [x] framebuffer/input/audio の最小 runtime probe binary を A30 上で動かす。
- [x] plumOS 同梱 SDL2 の最小 linked/window/input probe binary を A30 上で動かす。
- [x] plumOS 同梱 SDL2 の framebuffer/render backend を A30 上で検証し、upstream SDL3+sdl2-compat では実画面 backend がなく dummy/offscreen/evdev software renderer までであることを確認する。
- [x] stock SDL1/SDL2 の video backend を調査し、stock SDL2 は custom `mali` backend で `/dev/fb0` + Mali fbdev EGL 経路に出すことを確認する。
- [x] stock SDL2 の `mali` 挙動を参考に、stock SDL 非依存の fbdev + Mali EGL 最小 presenter probe を実装し、A30 上で `eglSwapBuffers` まで確認する。
- [x] SDL 実画面出力は当面 frontend 直結の fbdev + Mali EGL presenter を優先し、`plumos-controller-ui-mali` として A30 上で TOP 表示/ROM list 遷移まで確認する。
- [x] `plumos-controller-ui-mali` を A30 向け compact layout にし、TOP/ROM/Settings/SAFE の exercise と stock MainUI/keymon 併用 30 秒保持を確認する。
- [x] `plumos-controller-ui-mali` に `--rotation auto|none|cw|ccw` を追加し、A30 の `480x640` framebuffer では `auto` で stock と同じ raw 向きへ横画面 UI を描く。
- [x] Wi-Fi/SSH が stock `MainUI.stock`/`keymon` ではなく `wpa_supplicant`/`udhcpc`/`dropbear` で維持されることを確認する。
- [x] OS boot 時の Wi-Fi は `/etc/rc.d/S96wpa_supplicant` -> `/etc/init.d/wpa_supplicant` で起動することを確認する。
- [x] IP取得/DHCP は MainUI 側に依存していた可能性が高いため、FE 起動時に `plumos-network-rescue` を自動実行する方針にする。
- [x] START menu の `Network Recovery` と Network Settings の `Run Network Recovery` からも同じ network rescue を実行する。
- [x] UI Settings の write-enabled controls を controller UI の実挙動へ接続する。2026-06-09 に
  `Show Empty Systems`、`Favorites On TOP`、`Recent On TOP`、`Sort Systems`、
  `Sort ROMs`、`Scan On Enter`、`Boot Resume Mode` の保存と TOP/ROM/boot 挙動反映を
  実装した。`UI Mode` は Text/Graphic を保存し、`top_mode`/`rom_mode` へ同期する。
  Graphic 画面の実装は別タスク。
- [x] System Settings の項目をユーザー向けに整理する。2026-06-09 に第一階層を
  `Volume`、`Brightness`、`Lumination`、`Display Color`、`Language`、`Theme`、
  `INFORMATION` にし、現在値や backend/policy などの情報系は `INFORMATION` サブ項目へ
  集約した。`Device Model`、`Linux Kernel`、`SD Card`、`plumOS System Config`、
  `Input Device`、`Theme Source`、`Audio Backend`、
  `Display Backend`、`Write Policy` を INFORMATION に置き、Wi-Fi は Network Settings、
  CPU は Performance Settings へ分離する方針にした。
- [x] plumOS 設定ファイルの配置方針を `docs/plumos-config-layout.md` にまとめる。
  1本の system.json に集約せず、`config/frontend/`、`config/system/`、
  `config/network/`、`config/performance/` のように責務ごとに分ける。
  stockOS-owned な `/config/system.json` は plumOS 通常 UI の読み書き対象にしない。
- [x] System Settings の実動作を `/mnt/SDCARD/plumos/config/system/settings.json` の
  安全な限定 write に接続する。
  2026-06-09 に `Volume`、`Brightness`、`Lumination`、`Language`、`Display Color`
  サブ項目の `Contrast`、`Hue`、`Saturation` を左右キーで保存できるようにした。
  初回 backup (`.plumos.bak`)、tmp file、fsync、rename、sync を使う。`Theme` は read-only。
- [x] Performance Settings を既存の plumOS CPU profile 機能へ接続する。
  `plumos-text-ui core system|rom ... --cpu --freq --cores` と
  `/mnt/SDCARD/plumos/state/frontend/core-overrides.json` を使い、`systems.json` の
  `default_cpu_policy` / `default_cpu_freq_khz` を既定値として表示できるようにした。
  2026-06-09 に START `Performance Settings` へ `System`、`CPU freq`、
  `CPU Cores`、source表示、`Reset to Default` を追加した。
  左右で `System`、CPU frequency preset、2/4 core preset を変更し、保存は既存の
  `plumos-text-ui core system ...` 経由で `core-overrides.json` に入る。
  ユーザーが予測しづらい `keep` は削除し、Performance Settings の周波数候補は
  `648/816/1200/1344 MHz` の固定値だけにした。plumOS default は全systemで
  `648MHz` / `2 cores` に揃える。A30実機では `nes` を `816MHz` / `4 cores` の
  system override に変更後、`Reset to Default` で `648MHz` / `2 cores` の
  `plumOS default` へ戻ることを
  `frontend-performance.log` と SHA unchanged の最終 state で確認済み。
- [x] Network Settings の項目をユーザー向けに整理する。2026-06-09 に `Wi-Fi`、
  `Connection`、`IP Address`、`Signal`、`Link Speed`、`Frequency`、`SSH`、
  `Status Source`、`Config Source`、`Credentials`、`Run Network Recovery`、
  `Write Policy` を実装した。SSID/PSK は表示せず、A ボタンで recovery を実行するのは
  `Run Network Recovery` 行だけにした。
- [x] `scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock` で stock `/etc/main`、`MainUI.stock`、`keymon` を止めた plumOS 想定状態で Mali UI を確認する。
- [x] `/dev/fb0` capture で TOP/ROM/START/Settings/SAFE を確認し、START menu の Mali 表示を短縮して右端省略を減らす。
- [x] Mali renderer の常時表示を stock MainUI 寄せにし、上部は時刻/Wi-Fi/Batteryだけ、下部操作説明は削除、Settings に `HELP` 項目を追加する。
- [x] Mali renderer で日本語など非ASCIIのROM名を `???` にせず表示できるようにする。2026-06-08 に `libfreetype` をMali版controller UIへ動的リンクし、ASCIIは組み込みbitmap、非ASCIIは `PLUMOS_MALI_FONT` / theme font / A30既存CJK font 候補で描画するよう修正。UTF-8単位の切り詰めとMali ROM行のタイトル抽出も修正し、`artifacts/a30-probes/frontend-font-jp-clean2/20260608-215527.visible.cw.png` で `ドラゴンクエストIII`、`悪魔城伝説`、`ファイナルファンタジー` の表示を確認済み。
- [x] A30 実機 UI の文字サイズ・font・list column alignment ルールを `docs/a30-ui-design.md` にまとめる。`1x` 未満のユーザー可視文字は禁止し、主要項目は `2x` を基準にする。
- [ ] 現行 TTY prompt の横圧縮表示をやめ、`docs/a30-ui-design.md` の `1x` 最小文字サイズルールに合わせて折り返しまたは短縮表記へ変更する。
- [x] `plumos-controller-ui-mali` を実機画面で目視し、文字可読性、余白、配色、選択表示を最終調整する。2026-06-08 に TOP/ROM/NETWORK/SAFE の capture とユーザー目視で問題なしを確認済み。将来の theme polish は別タスクとして扱う。
- [x] `--rotation auto` の物理画面向きをユーザー目視で確認し、逆向きなら `cw`/`ccw` を切り替える。A30 `480x640` framebuffer では `auto` の向きで TOP/ROM/NETWORK/SAFE、RetroArch hotkey復帰後のROM listまでユーザー目視確認済み。
- [ ] SDL3/sdl2-compat custom video backend 化は frontend presenter の挙動が固まってから再判断する。
- [x] stock MainUI を停止または置き換えた状態で audio playback を再検証する。RetroArch practical OSS audio、ScummVM、EasyRPG、PCSX-ReARMed、DOSBox-Pure は stockless plumOS 経路で実聴確認済み。PPSSPP の audio/latency 設定もユーザー調整後の設定ファイルを plumOS 標準候補として固めたため完了扱い。
- [x] stock `keymon` を残す場合と直接 `/dev/input/event*` を読む場合を比較する。
- [x] `plumos-input-compare --all-events` で電源/左スティック押し込み以外の物理ボタンの code/action mapping を確定する。
- [x] `plumos-input-compare` で `EV_ABS` を表示し、左スティックが kernel input に露出するか確認する。
- [x] 左スティック押し込みを `plumos-input-compare --all-events` と `/dev/ttyS0` serial frame で確認し、通常 kernel input/6-byte serial frame には出ないことを記録する。
- [x] stockOS/spruceOS の calibration 保存内容と実機 event 結果から、左スティック押し込みは初期 plumOS では未接続/未対応として扱う。
- [x] spruceOS の A30 実装を参照し、`joystickinput` が `/dev/ttyS2` raw serial と `/config/joypad.config` を使う構成を確認する。
- [x] `/dev/ttyS*` raw data 確認用の `plumos-serial-joy-probe` を追加する。
- [x] A30 実機では `/dev/ttyS0` から 9600/8N1 の `ff b1 b2 b3 b4 fe` frame が出ることを確認する。
- [x] 左スティック操作時の serial frame 範囲を capture し、`axisYR`/`axisXR` が `/config/joypad.config` の min/max に近いことを確認する。
- [ ] 左スティックの上下左右を個別 capture し、`axisYR`/`axisXR` の X/Y と符号を確定する。
- [x] `/config/joypad.config` と `/dev/ttyS0` raw serial 経路を使う plumOS 用 `plumos-joystickd` の最小設計を記録する。
- [x] `/dev/ttyS0` raw frame を `/dev/uinput` の virtual `ABS_X`/`ABS_Y` に変換する `plumos-joystickd` 最小実装を追加する。
- [x] `plumos-joystickd` の virtual input device が A30 実機で `js0`/`event4` として作成されることを確認する。
- [x] `plumos-joystick-reader` を追加し、Linux joystick API と evdev で `plumos-joystickd` の virtual input を監視できるようにする。
- [x] `plumos-joystick-reader` を A30 実機で動かし、Linux joystick API と evdev から `js0`/`event4` を読めるか確認する。
- [x] 左スティック押し込み未対応方針に合わせ、`plumos-joystickd` の virtual device から仮の `BTN_THUMBL` を外す。
- [x] `plumos-joystick-reader` に `js0` device name/axes/buttons 表示を追加する。
- [x] stock RetroArch の build feature を確認し、SDL1 は有効、SDL2 と udev/evdev は無効であることを記録する。
- [x] `scripts/probe-a30-retroarch-joystick.sh` を追加し、`plumos-joystickd` 起動中の stock RetroArch SDL joystick log probe を再実行可能にする。
- [x] stock RetroArch log probe では SDL joypad driver までは確認できるが、`plumOS A30 Analog Stick` の autoconfig/接続ログは出ないことを記録する。
- [x] stock RetroArch 実操作で、Port1 Controls の bind 待ち受けでは `Axis -2`/`±2` として検出されるが、左スティックでメニューカーソルは動かないことを記録する。
- [x] stock PPSSPP が `miyoo282_xpad_inputd` を起動し、`/dev/ttyS0` + `/config/joypad.config` + `/dev/uinput` で `MIYOO Pad1` を作る経路を確認する。
- [x] `scripts/probe-a30-ppsspp-input.sh` を追加し、PPSSPP 起動中の analog input 経路を再調査可能にする。
- [x] stock PPSSPP は SDL2 GameController/Joystick 経路で Xbox 360 互換風の `MIYOO Pad1` (`045e:028e`, `js0`/`event4`, 8 axes/11 buttons) を読んでいることを記録する。
- [x] stock RetroArch/SDL1 では `plumos-joystickd` の axes-only/composite virtual input device が Linux joystick API では見えるが、RetroArch log では device/autoconfig まで確認できないことを検証する。
- [x] RetroArch 向け analog strategy は stock SDL1 依存ではなく、plumOS build の SDL2/evdev + buttons+axes composite virtual pad を優先する方針に決める。
- [x] `plumos-joystickd` に PPSSPP と同系統の buttons+axes composite virtual pad mode を実装する。
- [x] `scripts/probe-a30-joystickd-xbox.sh` を追加し、`--device-mode xbox` の実機確認を再実行可能にする。
- [x] `plumos-joystickd --device-mode xbox` が A30 実機で `js*`/`event*` に 8 axes / 11 buttons 相当として出るか確認する。
- [x] `plumos-joystickd --device-mode xbox` の L2/R2 は PPSSPP の Pause 割り当てでアナログ軸連続発火によりハングするため、`--trigger-mode buttons|axes` を追加し、既定を `buttons` に変更する。`buttons` では L2/R2 を `BTN_TL2`/`BTN_TR2` として出し、旧挙動は `--trigger-mode axes` で戻せる。armv7 static build と A30 deploy は実施済み。2026-06-07 のPPSSPP再起動確認では daemon cmdline が `--trigger-mode buttons`、`/proc/bus/input/devices` が `KEY=7fdb0000...` / `ABS=3001b` となり、L2/R2 button bit 追加かつ `ABS_Z/RZ` 除外を確認した。`scripts/probe-a30-joystickd-buttons.sh --trigger-mode buttons|axes` で両モードの実機確認も再実行できる。
- [x] `scripts/probe-a30-ppsspp-plumos-gamepad.sh` を追加し、stock input daemon なしの PPSSPP + plumOS gamepad 確認を再実行可能にする。
- [x] `plumos-joystickd --device-mode xbox` が PPSSPP/SDL2 GameController から認識され、GameController mapping が成功することを確認する。
- [x] stock MainUI/keymon、PPSSPP direct launch、stock RetroArch probe の短時間確認後に `plumos-joystickd --device-mode xbox` の stale process/device/fd が残らないことを確認する。
- [x] `scripts/probe-a30-joystickd-buttons.sh` を追加し、A/B/X/Y、D-pad、L/R、L2/R2、START/SELECT、Function が `plumOS A30 Gamepad` の button/hat/trigger event として転送されることを確認する。
- [x] `scripts/probe-a30-sdl2-gamepad.sh` を追加し、plumOS 同梱 upstream SDL3 3.4.10 + sdl2-compat 2.32.68 が `plumos-joystickd --device-mode xbox` の composite virtual pad を GameController として自動認識することを確認する。
- [x] `scripts/probe-a30-joystickd-residency.sh` を追加し、stockless plumOS 状態で `plumos-joystickd --device-mode xbox` 常駐中の FE/SDL2/PPSSPP direct launch と終了後の stale process/device/fd なしを確認する。
- [x] PPSSPP raw/direct launch で懸念していた `/dev/input/event3` と `/dev/ttyS0` 追加 open は、標準 `plumos-standalone-launch ppsspp` で launcher-owned `plumos-joystickd --device-mode xbox` を使い、PPSSPP 本体には `plumOS A30 Gamepad` の `event4` だけを渡す構成で抑止できることを確認する。
- [x] plumOS RetroArch build では stock SDL1 経路に依存せず、SDL2 joypad + composite virtual pad を優先案として検証する。evdev/linuxraw primary input は初期化失敗のため初期対象外にする。
- [ ] `/dev/mem` ADC 経路は stock calibration/test 画面由来の可能性として優先度を下げ、必要になった場合のみ再調査する。
- [x] Function button で開く SAFE menu prototype を controller UI に実装する。
- [ ] 電源ボタン短押しの event code と stock 側の sleep/shutdown 介入を安全に確認する。2026-06-08 に `mem` sleep中の電源ボタン短押しで手動復帰できることは確認済み。event code 自体のcaptureとstock側介入の切り分けは未実施。
- [x] Function button の direct safe shutdown/resume daemon 経路を RetroArch 実行中に検証する。2026-06-08 に外部コマンド `plumos-safe-shutdown --no-poweroff` から FE/text-ui-owned RetroArch launcher へ TERM を送り、resume-session保持まで成功。さらに controller UI の SAFE menu と START Shutdown から `plumos-safe-shutdown --no-poweroff` を呼ぶ経路も `artifacts/a30-probes/safe-shutdown/20260608-164612-controller-ui-safe-menu` で確認済み。2026-06-08 に `plumos-safe-hotkeyd` を追加し、`/dev/input/event3` (`gpio-keys-polled`) を非排他で監視して Function=`KEY_ESC` または `SIGUSR1` から `plumos-safe-shutdown --shutdown --no-poweroff` を起動できるようにした。`artifacts/a30-probes/safe-shutdown/20260608-165456-safe-hotkeyd-sigusr1-nes` では NES/RetroArch 実行中に `SIGUSR1` で同じtrigger pathを通し、safe shutdown、resume hold、CPU復元、FE再起動、残留なしまで確認済み。`artifacts/a30-probes/safe-shutdown/20260608-170909-text-ui-autohotkey-sigusr1-nes` では `plumos-text-ui launch --execute` が `plumos-safe-hotkeyd --oneshot` を自動起動する実運用寄りの経路でも同じ結果を確認済み。`scripts/probe-a30-safe-hotkeyd.sh` を追加し、`--trigger signal` は `artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes` で `SAVE_STATE_SLOT 999` / `.state999` まで再確認済み、`--trigger physical` は `artifacts/a30-probes/safe-shutdown/20260608-171641-text-ui-autohotkey-physical-nes` で `trigger source=key` として物理Function押下から同じ経路を確認済み。FE SAFE Sleep/Shutdown と START Shutdown は、検証済み backend に接続済み。SAFE Sleep は既定 `mem`、SAFE/START Shutdown は実 `poweroff auto`。`PLUMOS_CONTROLLER_SAFE_DRY_RUN=1` の dry-runでUI経路を確認し、SAFE Sleep は `wakeup_sec=0` の実UI経路から電源ボタン短押し復帰まで確認済み。残りはゲーム中overlay menuを出すか直接安全終了にするかのUX決定。
- [x] `scripts/probe-a30-safe-hotkeyd.sh` の終了後状態を stockless plumOS 前提に揃える。2026-06-08 の `20260608-181326-text-ui-autohotkey-signal-nes` は probe 自体は ok だったが、boot wrapper が `MainUI.stock` fallback へ落ちたため、`/etc/main`/wrapper/stock fallback の明示停止と残留検査を追加した。修正後は `20260608-181818-text-ui-autohotkey-signal-nes` で `residual_boot_mainui=`、FE復帰 PID 3016、`Result: ok`。
- [x] plumOS frontend 常用起動時は stock `keymon` を停止する方針にする。
- [ ] RetroArch 実行中に Function button または利用可能なら電源キーで Sleep/Shutdown/Cancel menu を表示する方法を調査する。Function button から direct safe shutdown daemon を起動する経路は実機確認済みなので、残りは overlay menu が必要か、直接安全終了を標準UXにするかの決定。

## Phase 7 - RetroArch and Core Runtime

- [x] stockOS 側の `Emu`/`RApp`/installed core を参考に、plumOS でビルドする emulator/core/standalone target inventory を作り、A30で満足に動く見込みで Class A/B/C に分類する。
- [x] build 時点で最新 stable の RetroArch release を確認し、RetroArch 1.22.2 minimal RGUI/Mali fbdev build を A30 の armv7 hard-float 環境向けに build/test する。
- [x] RetroArch minimal display probe を `/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ配置する。
- [x] RetroArch minimal display probe は `HOME=/mnt/SDCARD/plumos/retroarch/home` と plumOS 管理 config を使い、stock `HOME=/mnt/SDCARD/RetroArch` に依存しないことを確認する。
- [x] RetroArch 1.22.2 minimal build で `fbdev_mali` + GLES/EGL + RGUI 表示を確認し、A30 GL2 menu MVP patch + `--rotation ccw` で横向き表示をユーザー目視確認する。
- [x] stockOS RetroArch 1.16.0 の `--features` を取得し、plumOS実用 build option の keep/defer/disable 方針を整理する。
- [x] `artifacts/reference/nes` と `artifacts/reference/gb` の ROM set を A30 の `/mnt/SDCARD/Roms/FC`、`/mnt/SDCARD/Roms/GB` へ転送し、FDS BIOS を `/mnt/SDCARD/plumos/retroarch/system` へ配置する。
- [x] upstream HEAD の `fceumm` と `gambatte` を A30 armv7 hard-float向けに build し、`/mnt/SDCARD/plumos/retroarch/cores` へ配置する。
- [x] `scripts/probe-a30-libretro-cores.sh` を追加し、`fceumm` + NES と `gambatte` + GB が RetroArch minimal 上でROMをロードし、Mali fbdev表示ループに入ることを確認する。
- [x] `fceumm` と `gambatte` のゲーム画面をA30実機でユーザー目視確認する。
- [x] RetroArch practical runtime build で audio/input を検証する。OSS audio + SDL2 joypad + `plumos-joystickd --device-mode xbox` で画面/音/操作をユーザー目視確認済み。
- [x] RetroArch practical runtime を `/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ置き換える方針を決め、A30 へ deploy する。
- [x] practical RetroArch/core runtime でも `HOME=/mnt/SDCARD/RetroArch` に依存しない起動方法へ移行する。
- [ ] `--config` と override config で system/core ごとの差分を管理する。
- [x] FE の ROM 選択 A ボタンから実際の launch path を実行し、`plumos-retroarch-launch --cpu/--freq/--cores`、game終了、CPU状態復元を通し確認する。Aボタン経路と `plumos-text-ui launch --execute` の実装は追加済み。2026-06-08 に `scripts/probe-a30-fe-launch.sh` を追加し、FE script の `A` 経路から NES/RetroArch を短時間起動、watchdog停止、CPU復元、joystickd停止、FE復帰まで確認済み。さらに RetroArch hotkey を SELECT+B menu / SELECT+START quit に変更し、controller UI の復帰直後event drain/cooldown追加後、ユーザー目視で menu Quit と SELECT+START のどちらもFEへ正常復帰、`frontend-launch.log` の `execute: ok`、CPU restore、RetroArch/joystickd残留なしを確認済み。capture は `artifacts/a30-captures/fe-after-select-combo-confirmed-20260608/20260608-183839.visible.cw.png`。
- [x] Class A/B の libretro core 41本を upstream HEAD 中心で A30 armv7 hard-float 向けに build し、`dist/plumos-libretro-cores` に stage する。
- [ ] build 済み Class A/B core を A30 へ deploy し、system ごとに起動、performance、save/state、input、audio/video を段階検証する。
- [x] libretro core と standalone emulator は stockOS version ではなく upstream latest stable を初期候補にし、実機異常がある場合だけ stockOS 側 version/patch/build option と比較する方針にする。
- [x] PPSSPP、ScummVM、EasyRPG Player、DOSBox Staging、PCSX-ReARMed の standalone 試作 build を A30 armv7 hard-float 向けに行い、`dist/plumos-standalone-emulators` に stage する。
- [x] build 済み standalone emulator package の first-pass full validation として、画面、音、入力、終了、CPU profile、save/config path を emulator ごとに検証する。採用候補は PPSSPP、ScummVM、EasyRPG Player、PCSX-ReARMed。DOSBox Staging は検証後に通常配布/通常起動対象から外し、DOSBox-Pure libretro を標準にする。
- [ ] standalone launcher の終了処理を再検証する。2026-06-08 のPPSSPP完了後、PPSSPP/launcher process は残っていなかったが CPU が `userspace fixed 1344000 kHz / 4 cores` のまま残ったため、通常終了、TERM、KILL、SSH session切断、PPSSPP UI終了の各経路で CPU restore が必ず走るように修正する。2026-06-08 追加修正で standalone launcher の signal cleanup は child へ即TERM/KILL、CPU restore、owned joystickd停止、fb restore の順へ変更し、PPSSPP launcher に TERM 後1秒でKILLする実機試験では CPU が `ondemand 120000/1200000`、online `0-3` へ戻り、PPSSPP/joystickd 残留なしを確認済み。PPSSPP は強制停止で POSIX shm `/PPSSPP_ID` が残ると次回 `Secondary instance ... silencing audio` になったため、PPSSPP実体プロセスがいない場合だけ launcher が `/dev/shm/PPSSPP_ID` を消すようにし、`scripts/probe-a30-fe-launch.sh psp --run-sec 8 --settle-sec 2` の `artifacts/a30-probes/fe-launch/20260608-151410-psp` で secondary instance ログなし、shm残留なし、CPU restore、プロセス残留なしを確認済み。
- [x] standalone emulator package を A30 の `/mnt/SDCARD/plumos/emulators` へ deploy し、5本すべての dynamic dependency/CLI smoke を確認する。
- [x] `plumos-standalone-launch` を追加し、stock SDL2 `mali` backend 優先、plumOS SDL2 runtime保持、emulator別 HOME/XDG path、ScummVM/DOSBox/PPSSPP data path をまとめる。
- [x] `plumos-fb-restore` を追加し、PCSX-ReARMed SDL1/fbcon や PPSSPP 終了後に framebuffer geometry/RGBA mask/offset を復元できるようにする。
- [x] `scripts/capture-a30-framebuffer.sh` を追加し、A30 の `/dev/fb0` raw から visible PNG と90度回転previewを保存できるようにする。
- [x] `scripts/stop-a30-display-processes.sh` を追加し、`ld-linux-armhf` 経由で起動して process名が emulator名にならないPPSSPP等も `/dev/fb0` open fd/cmdline から停止できるようにする。
- [x] standalone emulator の終了後/停止後に FE へ必ず復帰する launch ownership を実装する。stockless plumOS 試験ではエミュ終了後に画面所有者がいないと黒画面になり、`/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali --rescue-network --rotation auto` を起動すると復旧することを2026-06-08に確認済み。2026-06-08 に FE ROM list の Aボタンから `plumos-text-ui launch --execute` を呼び、Mali renderer を停止して emulator subprocess へ渡し、戻り後に renderer を再初期化する経路を実装した。
- [ ] FE-owned launch path の実機通し確認を行う。PPSSPP/PCSX/ScummVM/EasyRPG/RetroArch で Aボタン起動、ゲーム終了、CPU restore、joystickd停止、FE復帰、`logs/frontend-launch.log` を確認する。2026-06-08 に `scripts/probe-a30-fe-launch.sh` で PSP/PPSSPP、PSX/PCSX-ReARMed、EasyRPG、ScummVM、NES/RetroArch の FE `A` launch、短時間起動、watchdog停止、CPU restore、joystickd停止、FE復帰を確認済み。ログは `artifacts/a30-probes/fe-launch/20260608-150031-scummvm`、`20260608-151410-psp`、`20260608-150102-psx`、`20260608-150159-easyrpg`、`20260608-145647-nes` など。NES/RetroArch は `PLUMOS_RA_MAX_FRAMES=180` の自己終了で `artifacts/a30-probes/fe-launch/20260608-151740-nes` に `execute: ok`、CPU restore、joystickd停止、FE復帰を確認済み。2026-06-08 の入力drain修正後、probe のCPU判定を固定baselineではなく実行前後policy比較に直し、PSP/PSX/EasyRPG/ScummVMを再実行した。`artifacts/a30-probes/fe-launch/20260608-184054-psp`、`20260608-184113-psx`、`20260608-184131-easyrpg`、`20260608-184150-scummvm` はすべて `result=fe_launch_probe_ok`、`launch_started=yes`、`fe_script_exited=yes`、CPU policy復元、残留なし。capture `artifacts/a30-captures/after-standalone-fe-probes-20260608/20260608-184236.visible.cw.png` で通常TOP復帰を確認済み。残りは standalone 各 emulator のユーザー操作による通常終了確認。
- [x] ScummVM、DOSBox Staging、EasyRPG Player、PPSSPP、PCSX-ReARMed standalone の A30 実画面 smoke を確認する。PPSSPP は `/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso` boot、PCSX-ReARMed は `/mnt/SDCARD/Roms/PS/chroQW.cue` boot まで確認済み。
- [x] PPSSPP standalone の横向き表示を、stale PPSSPP process がない状態で単独再現する。2026-06-08 の全体回転版では、`plumos-standalone-launch ppsspp` は既定で `PLUMOS_A30_DISPLAY_ROTATION=ccw`、`PLUMOS_A30_DISPLAY_SWAP=0`、`PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=1`、`PLUMOS_A30_UI_ROTATION=none` を使う。PPSSPP設定を阻害しない方針にしたため、通常起動では `--appendconfig` を渡さない。旧append profileは `PLUMOS_A30_PSP_APPEND_PROFILE=1` の明示指定時だけ有効。`dist/plumos-standalone-emulators-ppsspp-logical-ui-scale-test` をbuild/deployし、cmdlineから `--appendconfig` が消え、`UIScaleFactor=-2` の保存設定が `ui_scale=0.841` として使われることを確認済み。
- [x] PPSSPP standalone は `plumos-standalone-launch ppsspp` で `plumos-joystickd --device-mode xbox` を自動起動し、既存常駐daemonがある場合は二重起動しない。起動中は `plumos-joystickd` が `/dev/ttyS0`、`/dev/uinput`、`/dev/input/event3` を開き、PPSSPP本体は `/dev/input/event4` と `/dev/fb0` のみを開くことを確認済み。
- [x] PPSSPP standalone の初回 `controls.ini` をランチャー側で seed し、既存ユーザー設定がある場合は上書きしないようにする。
- [x] stock PPSSPP の `controls.ini` は `L = 10-193`、`R = 10-192`、`Pause = 10-4008`、`Exit App = 10-4`。plumOS ではL2/R2をボタン化したため、PPSSPPメニュー呼び出しは `--shoulder-layout user` + `Pause = 10-104` を既定にする。Function button は PPSSPP hang 回避のため既定 `none` にする。
- [x] PPSSPP menu/UI の縦向き対策として、RetroArchと同じくアプリ側の最終表示回転を使う方針へ変更した。`PLUMOS_A30_DISPLAY_ROTATION=ccw` でPPSSPP全体を回し、GLES UI text がscissorで落ちる問題は、回転中の `UIContext::ActivateTopScissor()` を全画面scissorに逃がして回避した。`PLUMOS_A30_DISPLAY_SWAP=1` はメニュー文字は出るが右側に未使用領域が残るため、既定は `swap=0`。
- [x] PPSSPP menu/UI は `PLUMOS_A30_DISPLAY_LOGICAL=854x480` と `UIScaleFactor` の組み合わせで視認性を調整できるようにした。`UIScaleFactor=-8` では `dp=1708x960`、`UIScaleFactor=-2` では `dp=1016x571` となることをログ確認済み。`UIContext::SetBounds()` で16:9化する案は `EmuScreen::update()` の `PSP_CoreParameter().pixelWidth/Height` に漏れてL2 menuを壊すため採用しない。
- [x] PPSSPP game画面のアスペクト試験として、通常の保存設定側の `[DisplayLayout.Landscape] DisplayAspectRatio = 0.562500` に変更し、append profileなしで停止・再起動した。起動中logでは `aspect=0.5625`、`orig_ratio=0.9926`、`rc=481.0x484.0+0.0+78.0` として反映済み。capture は `artifacts/a30-captures/ppsspp-game-aspect-05625/20260608-054244/`。
- [ ] FE のPPSSPP設定救済機能として、PPSSPP config/controls の backup、restore、factory reset を実装する。plumOSはPPSSPP/RetroArchなどエミュ本体の設定を基本的にユーザーへ開放し、設定が壊れた場合はFE側の救済操作で戻せる設計にする。仕様検討項目: 標準設定の正式配置先、ユーザーbackupの世代管理、restore対象ファイル、factory reset前の自動backup、破損ini検知、RetroArch等への横展開。
- [x] PPSSPP standalone のユーザー調整後設定を plumOS 標準/ファクトリーリセット候補として `artifacts/plumos-factory-defaults/ppsspp/20260608-055744/active/PSP/SYSTEM/` に退避した。対象は実機の `/mnt/SDCARD/plumos/state/standalone/ppsspp/config/ppsspp/PSP/SYSTEM/{ppsspp.ini,controls.ini}`。`current` symlink と `SHA256SUMS` も作成済み。参考として `$HOME/.config` 相当の旧配置も `legacy-home-dot-config/` に保存した。
- [x] stock PPSSPP をROMなしで直接起動し、stock SDL/Mali経路では物理 `480x640` mode のままPPSSPPメニューが横向きに正しく表示されることを確認した。capture は `artifacts/a30-captures/stock-ppsspp-menu/20260608-042921/current-cw.png`。stock live binary は `/home/raymanfeng/home3/miyoo_a33/emu/ppsspp-1.16.6` 由来の文字列を持つが、公式 v1.16.6 に無い `current_mode`、`mali creation succedded`、serial joystick log を出すため、Miyoo/Raymanfeng 側の独自SDL/GLES/input patch が入っている可能性が高い。
- [ ] stock PPSSPP の横メニュー成立要因を、公式 PPSSPP v1.16.6/v1.20.4 の `SDL/SDLMain.cpp`、`SDLGLGraphicsContext`、`Common/System/Display.*` と比較して切り出す。`UIContext::SetBounds()` で16:9化する案は `EmuScreen::update()` の `PSP_CoreParameter().pixelWidth/Height` に漏れてL2 menuを壊すため採用しない。
- [x] PPSSPP menu key は `PLUMOS_A30_INPUT_TRACE=1` 付きの実機probeで再確認した。旧 `plumos-joystickd` では L2 が SDL `controller_axis axis=4` として `Pause` を発火し、L は SDL `controller_button button=9 mapped_key=193` としてPSP Lに入るだけだった。その後の手動確認で L/R を `Pause` に割り当てると正常にメニューが開き、L2/R2 を `Pause` に割り当てるとハングしたため、L2/R2 のアナログ軸扱いをやめてボタン扱いにする方針へ変更した。
- [x] PPSSPP/L2 混乱の切り分けとして、PPSSPPを停止し、plumOS入力層の全ボタン採取を `artifacts/a30-probes/input-map/20260608-000711/remote.log` と `summary.md` に保存した。`--shoulder-layout standard` では物理 L2/R2 は raw `KEY_E/KEY_T` -> virtual `BTN_TL2/BTN_TR2` -> JS `b6/b7`。SDL 2.26.1 が見る実GUIDは `030003f05e0400008e0200005e040000` で、旧 `SDL_GAMECONTROLLERCONFIG` のGUID `0300e4fb...` が一致せず内蔵 `Atari Xbox 360 Game Controller` mapping に落ちていた。launcher の PPSSPP 既定 mapping を実GUIDへ修正し、L2/R2 は `leftstick:b6/rightstick:b7`、shoulder layout は `standard` に戻した。
- [x] 2026-06-08 の追加切り分けで、標準PPSSPP起動中にPPSSPP本体が開いている入力fdは `/dev/input/event4` (`plumOS A30 Gamepad`) のみで、raw `/dev/input/event3` (`gpio-keys-polled`) は開いていないことを確認した。一方、ユーザーがL2として押した左肩ボタンは raw 採取で `KEY_TAB`/code `15` として出ており、`--shoulder-layout standard` では virtual `BTN_TL`/JS `b4`、PPSSPP trace では `controller_button button=9 mapped_key=193` としてL扱いになる。左側のもう一つの肩ボタンは raw `KEY_E`/code `18` として出るため、A30物理ラベル/ユーザー呼称と `standard`/`user` shoulder layout の対応を再整理する。
- [x] PPSSPP のL2メニューは、`--shoulder-layout user` でユーザーがL2として押す物理 `KEY_TAB` を SDL `LEFTSTICK` -> PPSSPP `NKCODE_BUTTON_L2` (`mapped_key=104`) に入れ、`controls.ini` の `Pause = 10-104` へ割り当てる構成でユーザー目視確認済み。`--escape-exit` が付いているとPause発火時に即終了してハングに見えるため、launcher既定から外し、必要な場合だけ `PLUMOS_A30_PSP_ESCAPE_EXIT=1` で有効化する。
- [x] PPSSPP 1.20.4 の改造影響を切り分けるため、`PPSSPP_PATCH_MODE=vanilla` と `PPSSPP_STAGE_ID=ppsspp-vanilla` で通常PPSSPPを別build/deployできるようにした。`/mnt/SDCARD/plumos/emulators/ppsspp-vanilla` と別state `/mnt/SDCARD/plumos/state/standalone/ppsspp-vanilla` で起動する。plumOS SDL3+sdl2-compat 優先では `mali not available` でSDL初期化に失敗するため、PPSSPP standalone は現時点では stock SDL2 `mali` backend fallback が必要。stock SDL fallback では `/mnt/SDCARD/Roms/PSP/Puzzle_Bobble.cso` boot と `/dev/fb0` open を確認済み。
- [ ] 通常PPSSPP `ppsspp-vanilla` で、SELECT/L/L2/R2 の Pause/menu/hang 挙動をユーザー目視で確認し、改造版PPSSPPとの差分を整理する。
- [x] PPSSPP standalone の launcher TERM 時に、`ld-linux-armhf` 経由のPPSSPP本体と launcher-owned `plumos-joystickd` が残らないことを確認する。`run_ld` shell function 経由の孫process orphan は修正済み。
- [x] PPSSPP standalone の既定CPU profileを `userspace fixed 1344000 kHz / 4 cores` にし、起動中に `ondemand` を避ける。終了時は元のCPU状態へ復元できることを短時間実機確認済み。
- [x] `scripts/probe-a30-ppsspp-standalone-runtime.sh` を追加し、標準PPSSPP launcher経由でCPU固定、joystickd自動起動、fd所有、入力、PPSSPP log、終了後復元を一括確認できるようにする。
- [x] PPSSPP standalone の ALSA audio underrun と CPU profile/latency 設定を長めの実ゲームで確認する。append profile 廃止により `ExtraAudioBuffering=True`、`AudioBufferSize=1024` などは通常起動で強制しない。2026-06-08 にユーザー調整後の設定ファイルが固まったため完了扱い。音途切れが再発する場合は、FEの推奨設定適用/プリセット選択として扱い、ユーザー設定を無断上書きしない。
- [x] ScummVM、EasyRPG、DOSBox Staging は SDL2/keyboard/mouse/gamepad profile と音声 backend の実機挙動を確認する。ScummVM/EasyRPG は採用候補として問題なし、DOSBox Staging は動作するが負荷/音切れから通常対象外。
- [x] ScummVM 起動確認用に `artifacts/reference/scummvm/BASS-Floppy-1.3` を `/mnt/SDCARD/Roms/SCUMMVM/BASS-Floppy-1.3` へ転送し、`plumos-standalone-launch scummvm --path=/mnt/SDCARD/Roms/SCUMMVM/BASS-Floppy-1.3 sky` で素起動した。log は `current_mode 480x640` / `MALI_CreateWindow` / `Found BASS version` まで到達。capture は `artifacts/a30-captures/scummvm-bass-plain/20260608-060250/`。結果はA30物理縦画面へそのまま描画され、横持ちではゲーム画面が横倒しになる。
- [x] ScummVM SDL Surface backend の既存 `rotation_mode` を試し、`rotation_mode=270` が既存PPSSPP横向きcaptureと同じ向きになることを確認した。`fullscreen=true` で表示領域は広がるが、BASSの320x200画面は縦長寄りにスケールされるため、`stretch_mode`/aspectの最終値は追加調整する。
- [x] ScummVM standalone launcher に A30初回config注入とjoystickd自動起動を追加した。`--config`明示時はユーザー指定を優先し、既定configは `/mnt/SDCARD/plumos/state/standalone/scummvm/config/scummvm.ini` が無い時だけ作成する。ScummVM用joystickd既定は raw採取結果に基づき `x-source=axisYR` / `y-source=axisXR`。
- [x] ScummVM directory型ROMの target id を sidecar/manifest で持たせる。2026-06-08 に `plumos-standalone-launch scummvm <directory>` が directory内 `.plumos-scummvm-target`、`scummvm-target.txt`、`.scummvm`、隣接 `.scummvm`/`.svm` から target id を読むようにし、`PLUMOS_A30_SCUMMVM_TARGET` は明示overrideとして残した。target id は英数字、`.`、`_`、`-` のみ許可し、sidecar が無い場合だけ既存検証ROM向けに `sky` へ fallback する。A30には `/mnt/SDCARD/Roms/SCUMMVM/BASS-Floppy-1.3/.plumos-scummvm-target` (`target=sky`) を配置し、`artifacts/a30-probes/fe-launch/20260608-185353-scummvm` で FE Aボタン経路、`User picked target 'sky'`、CPU policy復元、joystickd停止、FE復帰を確認済み。
- [x] ScummVM `rotation_mode=270` 時に VirtualMouse の `warpMouse()` が画面端へclipされる問題を、`convertVirtualToWindow()` の回転座標逆変換修正で解消した。patch は `docker/plumos-toolchain/patches/scummvm-2026.2.0-rotation-mouse-warp.patch`。2026-06-08 にユーザー目視でアナログ操作によるマウスカーソル移動OKを確認した。
- [x] ScummVM menu文字サイズは `gui_scale` 方式だと 125% で低解像度layoutへ落ちて崩れるため不採用。`scummmodern-a30-md` theme を作成し、A30標準候補として採用した。実機configは `gui_theme=scummmodern-a30-md`。MD theme は font 19pt / line 25 / button 35 で、ユーザー目視OK。
- [x] ScummVMのメニュー操作、ゲーム内クリック、終了動線、audio underrun有無をユーザー目視で再確認する。2026-06-08 にユーザー目視で問題なしを確認済み。
- [x] EasyRPG Player 0.8.1.1 に A30 display rotation patch と実用音声/補助機能を追加した。Docker toolchain に `libexpat1-dev:armhf`、`libicu-dev:armhf`、`libharfbuzz-dev:armhf`、`liblhasa-dev:armhf`、`libmpg123-dev:armhf`、`libsndfile1-dev:armhf`、`libxmp-dev:armhf`、`libxml2-dev:armhf` などを追加し、build log で `MP3 playback: mpg123`、`WAV playback: built-in (dr_wav);libsndfile`、`Ogg Vorbis playback: libvorbis`、`Opus playback: opusfile`、`MOD playback: libxmp`、`LZH archive support: lhasa`、`Font rendering: Freetype with Harfbuzz / built-in` を確認した。2026-06-08 の `TurnedIntoAGirl` 起動では title capture から `Format not supported` 表示が消え、同時刻以降の EasyRPG log に MP3警告なし。
- [x] EasyRPG Player の音声実聴、gamepad入力、終了動線、save/config path をユーザー目視で確認する。2026-06-08 にユーザー目視で動作問題なしを確認済み。MIDI は現時点では built-in FmMidi。高品質MIDIが必要な場合は WildMIDI/FluidSynth と soundfont/config path をFE設定として検討する。
- [x] DOSBox Staging 0.82.2 に A30 display rotation patch を追加し、`PLUMOS_A30_DOSBOX_ROTATION=ccw` を launcher 既定にした。正式build/deploy後、`DOSBOX_TV.ZIP` の3D Realms logo と `DOSBOX_ALIENBREED.ZIP` のmenuが `/dev/fb0` capture上で横向きに表示されることを確認済み。
- [x] DOSBox Staging の実ゲーム継続表示、入力mapping、音声、終了動線を安定する確認用ROMで再検証した。2026-06-08 の切り分けでは direct `/dev/input/event3` patch で Digger 操作は通ったが、`texture_renderer=software` はロード画面から描画が重く音切れし、`texture_renderer=opengles2` は音声負荷は改善するものの standalone Staging は主処理が1コア上限に近い。build option は `dynamic_core=dynrec`、`per_page_w_or_x=disabled`、ARMv7/NEON CFLAGS を既に適用済みで、追加の build option だけで大幅改善する余地は小さい。
- [x] DOSBox-Pure libretro を A30 へ追加deployし、RetroArch practical から `DOSBOX_DIGGER.ZIP#DIGGER.EXE`、`DOSBOX_ALIENBREED.ZIP#ALIEN.EXE`、`DOSBOX_DOOM.ZIP#DOOM.EXE` を起動確認した。`audio_driver=oss`、`audio_latency=256`、`dosbox_pure_force60fps=true`、`dosbox_pure_audiorate=48000`、`dosbox_pure_midi=disabled`、CPU `fixed 1344000 / 4 cores` で Digger/DOOM はユーザー目視・実聴で問題なし。DOOM の5秒sampleは全体CPU約25%、RetroArch 1コア換算約97%。
- [x] FE 側の DOS system は DOSBox-Pure libretro を標準にし、standalone DOSBox Staging は plumOS の通常配布/通常起動対象から除外する。standalone DOSBox Staging は SDL2/mali renderer 相性・直接入力patchの検証アーティファクト扱いで残す。
- [x] DOSBox-Pure 用に、ROM/ZIP 内の起動EXEを `#EXE` suffix で指定し、game別 profile として OSS/audio latency/force60fps/cycles/CPU profile を保存できるようにする。2026-06-08 に `core-overrides.json` の per-ROM override へ `content_suffix`、`audio_driver`、`audio_latency_ms`、`dosbox_pure_force60fps`、`dosbox_pure_cycles` を追加し、`plumos-retroarch-launch` が `--audio-latency` / `--dosbox-pure-force60fps` / `--dosbox-pure-cycles` を一時 append config へ書くようにした。A30 では `DOS/DOSBOX_DIGGER.ZIP` に `#DIGGER.EXE`、OSS latency 256、force60fps、cycles max、CPU fixed 1344MHz/4core を保存し、`artifacts/a30-probes/fe-launch/20260608-205645-dos` で FE Aボタン経路、`execute: ok`、CPU restore、残留なしを確認済み。
- [x] RetroArch/DOSBox-Pure の起動は FE/launcher が必ず子プロセスとして所有し、終了時に `CLOSE_CONTENT`/`QUIT` または TERM/KILL、joystickd停止、CPU restore、FE復帰まで実行する。2026-06-08 の手動試験では direct `start-stop-daemon -b` 起動した RetroArch が残存し、`stop-a30-display-processes.sh` で PID 3067 を停止した。2026-06-08 追加修正で `plumos-retroarch-launch` は RetroArch を background child として保持し、cleanup 冒頭で CPU restore、RetroArch child/joystickd へ即TERM/KILL、fallback joystickd scan を行う。launcher に TERM 後1秒でKILLする実機試験では CPU/cores 復元、RetroArch/joystickd 残留なし、FE再起動を確認済み。probe用に `PLUMOS_RA_MAX_FRAMES` / `--max-frames` を追加し、NES/RetroArch の FE-owned launch で RetroArch 自己終了、`execute: ok`、CPU/cores復元、joystickd残留なしを確認済み。さらに loopback 自動UP修正後、FE-owned NCI `QUIT` でも `artifacts/a30-probes/fe-launch/20260608-160237-nes` に `execute: ok`、CPU restore、joystickd停止、FE復帰を確認済み。2026-06-08 に `plumos-retroarch-launch` の TERM cleanup へ `SAVE_STATE` -> `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` のNCI安全終了を追加し、`artifacts/a30-probes/retroarch-netcmd/20260608-162406-nes-launcher-term-safe-exit` で launcher TERM だけを起点に `.state` 更新、`GET_STATUS CONTENTLESS`、CPU restore、FE復帰まで確認済み。その後、RetroArch に plumOS patch `SAVE_STATE_SLOT` を追加し、launcher 既定を `SAVE_STATE_SLOT 999` -> `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` に変更。`artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes` で `.state999` 作成、resume hold、CPU restore、残留なしまで確認済み。FE SAFE/START shutdown flow と実 `poweroff` も統合・確認済み。DOSBox-Pure は RetroArch/libretro 経路で同じ ownership/cleanup に乗る。DOSBox-Pure固有profileは別タスクで継続する。
- [x] RetroArch のメニュー/終了 hotkey を Function 単独から SELECT combo へ移す。2026-06-08 に RetroArch practical config と SDL2 autoconfig を更新し、SELECT=`input_enable_hotkey_btn 4`、SELECT+B=`input_menu_toggle_btn 0`、SELECT+START=`input_exit_emulator_btn 6`、`quit_press_twice=false` として A30 へ deploy 済み。Function は FE/SAFE 側に残す。初回目視では RetroArch終了直後にSELECT/B/STARTの入力がFEへ漏れ、menu quit後に自動再起動、SELECT+START後にCORE画面へ遷移したため、controller UI の emulator復帰直後にevent fd drainと750ms cooldownを追加/deployした。修正後、ユーザー目視でSELECT+B menu、menu Quit -> FE復帰、SELECT+START -> FE復帰の操作に問題なしを確認済み。`frontend-launch.log` は複数回 `execute: ok`、RetroArch/joystickd残留なし、capture `artifacts/a30-captures/fe-after-select-combo-confirmed-20260608/20260608-183839.visible.cw.png` でROM list復帰を確認。
- [x] PCSX-ReARMed standalone は PS1 ROM/BIOS を用意して、画面、音、入力、終了、save/config path を確認する。A30向けには native fb32 rotation、SWSURFACE/doublebuf抑止、menu internal 640x480 landscape virtual、Function menu open/return、shadow framebuffer clear、keyboard profile passthrough を採用し、ユーザー目視でゲーム画面/音/操作/メニュー復帰OKを確認済み。
- [ ] 起動、performance、save/state、input、audio/video を core ごとに検証する。
- [ ] RetroArch の Auto Save State / Auto Load State を plumOS resume flow で検証する。plumOS safe shutdown 経路は dedicated slot 方式に寄せ、2026-06-08 に `PLUMOS_RA_SAFE_STATE_SLOT` / `--safe-state-slot` 既定 `999`、NCI `SAVE_STATE_SLOT 999`、resume plan の `--entry-slot 999` まで接続・実機確認済み。RetroArch 本来の `savestate_auto_save`/`savestate_auto_load` 設定を使う flow は未検証。
- [x] Network Control Interface または同等手段で `SAVE_STATE`、`CLOSE_CONTENT`、`QUIT` を安全に実行できるか確認する。2026-06-08 に静的 helper `plumos-udp-send` を `dist/plumos-runtime-probe` へ追加し、A30へdeploy済み。初期確認では `GET_STATUS` timeout/`QUIT` 無効だったが、`strace`/`ss` 追加後の切り分けで RetroArch は `0.0.0.0:55355` へ UDP bind 済み、`ss -lunp` でもsocketが見えることを確認した。根本原因はA30の `lo` がDOWNで、既定送信先 `127.0.0.1` からpacketが届かなかったこと。確認artifactは `artifacts/a30-probes/retroarch-netcmd/20260608-160000`。`plumos-retroarch-launch` は RetroArch 起動前に `ip link set lo up` を実行するよう修正/deploy済み。手動lo-up確認 `artifacts/a30-probes/retroarch-netcmd/20260608-160103-lo-up` と launcher自己lo-up確認 `artifacts/a30-probes/retroarch-netcmd/20260608-160214-launcher-lo` では `plumos-udp-send --recv-ms 1500 GET_STATUS` が `PLAYING ... crc32=e47960e0` を返し、`QUIT` でlauncher終了、CPU restore、FE復帰まで確認済み。FE-owned `QUIT` は `artifacts/a30-probes/fe-launch/20260608-160237-nes` で `execute: ok`。`scripts/probe-a30-retroarch-netcmd.sh` を追加し、direct launcher 経路で `SAVE_STATE` -> `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` を確認済み。`artifacts/a30-probes/retroarch-netcmd/20260608-161124-nes-save-close` では FCEUmm の `.state` が新規作成され、`CLOSE_CONTENT` 後に `GET_STATUS CONTENTLESS`、最後にCPU restoreとFE復帰まで成功。
- [x] shutdown 前に save state、save RAM flush、RetroArch 終了、`sync` を順に実行する安全終了 script を作る。RetroArch launcher の TERM cleanup では `SAVE_STATE_SLOT 999` -> `SAVE_FILES` -> `CLOSE_CONTENT` -> `QUIT` -> fallback TERM/KILL を実装/deploy済み。2026-06-08 に `plumos-safe-shutdown` と `plumos-text-ui` の resume hold を追加/deployし、`artifacts/a30-probes/safe-shutdown/20260608-163834-text-ui-nes` で FE/text-ui-owned NES 起動中に `plumos-safe-shutdown --shutdown --no-poweroff` を実行、launcher TERM から `.state` 更新、`GET_STATUS CONTENTLESS`、CPU restore、`sync`、`resume-session.json` の `pending=true` / `reason=shutdown` / `auto_state_load=true`、`execute: safe-exit`、FE再起動まで確認済み。`artifacts/a30-probes/safe-shutdown/20260608-164612-controller-ui-safe-menu` では controller UI の SAFE Sleep、SAFE Shutdown、START Shutdown がそれぞれ `plumos-safe-shutdown --no-poweroff` を呼び、`result=plumos_controller_ui_safe_shutdown_ok` まで確認済み。`artifacts/a30-probes/safe-shutdown/20260608-165456-safe-hotkeyd-sigusr1-nes` と `20260608-170909-text-ui-autohotkey-sigusr1-nes` では `plumos-safe-hotkeyd` の `SIGUSR1` triggerから同じ安全終了が成功。2026-06-08 に dedicated safe state slot 999 を追加し、`artifacts/a30-probes/safe-shutdown/20260608-173024-text-ui-autohotkey-signal-nes` で `SAVE_STATE_SLOT 999`、`.state999` 作成、`resume-session.json` pending/auto_state_load、`boot_resume_mode=last` 時の `--entry-slot 999` plan まで確認済み。さらに `plumos-safe-shutdown` に `--power-backend auto|poweroff|halt|busybox|sysrq|none`、`--sleep-backend pseudo|standby|mem|bootfast|none`、`--wakeup-sec` を追加し、`scripts/probe-a30-safe-power.sh` の `artifacts/a30-probes/safe-power/20260608-173951-dry-run` で poweroff/sleep/RTC wakealarm の dry-run を確認済み。実 `standby`/`mem` suspend と実 `poweroff` も発火済みで、復帰/電源ON後のWi-Fi/SSH/FE NETWORK画面まで確認済み。
- [x] A30 で本物の suspend/resume が可能か確認し、難しい場合は疑似 sleep 方針を検討する。A30 は `/sys/power/state` に `standby mem bootfast` を出し、`rtc0/wakealarm` も writable。2026-06-08 に `--sleep-backend standby --wakeup-sec 5` と `--sleep-backend mem --wakeup-sec 10` の dry-run で wakealarm 設定予定値と `/sys/power/state` 書き込み予定ログを確認済み。同日に `--sleep-backend standby --wakeup-sec 10` を実発火し、約12秒で復帰、Wi-Fi/SSH、CPU復元、FE NETWORK画面表示まで確認済み。ユーザー目視でも、standby中の画面消灯と自動復帰後のFE NETWORK画面表示を確認済み。続けて `--sleep-backend mem --wakeup-sec 10` も実発火し、約13秒で復帰、Wi-Fi/SSH、CPU復元、FE NETWORK画面表示まで確認済み。ユーザー目視でも、mem sleep中の画面消灯と自動復帰後のFE NETWORK画面表示を確認済み。さらに FE SAFE Sleep の既定 `mem` / `wakeup_sec=0` を実行し、画面暗転後に電源ボタン短押しで復帰、Wi-Fi/SSH、CPU、FE NETWORK画面維持まで確認済み。初期UXは疑似sleepではなく `mem` sleepを標準候補にする。

## Phase 8 - A30 System Policy Validation

- [x] CPU governor、CPU online/offline、clock policy を計測する。
- [x] stock script の CPU2/CPU3 offline が妥当か判断する。2コア/4コア dummy max load 測定では平均電力が約1978mW/約8066mWで差が大きいため、低負荷systemの既定は2コア寄り、重いsystemだけFEのCPU core presetで4コア指定する方針にする。
- [x] `performance`, `ondemand`, `userspace fixed` governor を比較する。NES/GB では `performance` と fixed 648MHz は動作良好、`ondemand` は音途切れが発生。
- [x] game 終了後に CPU 状態を確実に戻す仕組みを `plumos-retroarch-launch`/probe に入れる。
- [x] FE の SELECT core menu から system/ROM 別の CPU frequency/core preset を見られるようにし、実起動では `plumos-retroarch-launch --cpu/--freq/--cores` へ解決する。
- [x] バッテリー駆動で 2コア + performance + dummy max load 120秒と 4コア + performance + dummy max load 120秒を測定する。平均電力は2コア約1978mW、4コア約8066mWで、4コアは約4.1倍。
- [x] Wi-Fi の power sequence を plumOS 側で安全に再現できるか確認する。boot wrapper が `plumos-network-rescue` を先に起動し、poweroff -> 電源ON後も DHCP attempt 1でIP取得、Dropbear復旧、FE NETWORK表示まで複数回確認済み。
- [x] poweroff 後の電源ONで、起動直後の `wpa_supplicant`、DHCP、IP取得タイミングを `network-rescue.log` で追跡する。
- [x] Wi-Fi 自動接続を複数回 poweroff -> 電源ONで確認し、DHCP retry 回数/待ち時間を調整する。2026-06-08 に実 poweroff -> 電源ONを複数回実施し、いずれも DHCP attempt 1で `192.168.10.165` を取得、Dropbear復旧まで確認済み。現時点で retry 回数/待ち時間の追加調整は不要。
- [ ] stock Wi-Fi userland を使い続けるか、plumOS 同梱へ移すか判断する。
- [ ] SSH を開発用 package のままにするか、plumOS service にするか決める。

## Phase 9 - Packaging and Release

- [ ] 開発用 Docker 関連ファイルも成果物/配布物の一部として扱う。
- [ ] SD カードへ展開する runtime package を作る。
- [ ] 開発者向け Docker/toolchain package を作る。
- [ ] end-user 向け release と developer 向け release の分け方を決める。
- [ ] StockOS と plumOS の差分を整理する。例: stock RetroArch は SDL1有効/SDL2無効、plumOS は upstream RetroArch/core、SDL2/evdev + `plumos-joystickd`、stock MainUI/keymon 非依存、SDカード配下 runtime 管理。
- [ ] plumOS 導入メリットを end-user 向けにまとめる。例: 最新寄り emulator/core、FEでの core/launch profile 選択、stockカテゴリ非依存、resume/safe shutdown 方針、Wi-Fi/SSH rescue、差し戻しやすいSDカード中心設計。
- [ ] StockOS から plumOS へ移行する際の既知の制約/トレードオフを整理する。例: audio/input/full RetroArch runtime は段階検証中、A30 reboot挙動は未確定、stock互換ではなくplumOS独自仕様として設計する点。
- [ ] license notice と upstream attribution を整理する。
- [ ] GitHub Release 用の package/artifact workflow を作る。
- [ ] fresh SD card 相当の環境で install/rollback を検証する。
