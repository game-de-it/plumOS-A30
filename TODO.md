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
- [ ] Docker 内で frontend、helper、RetroArch、libretro core を build できるようにする。
- [x] Docker 内で frontend compatibility scanner を build できるようにする。
- [x] Docker build output を `dist/` または staging directory へ集約する。
- [x] build cache と大きな生成物が git に入らないよう ignore/配置を整理する。
- [x] Docker image の作り方と使い方を日本語/英語で文書化する。

## Phase 2 - plumOS Runtime Layout

- [ ] `/mnt/SDCARD/plumos` 配下の runtime directory 構成を package 化する。
- [ ] `bin/`, `lib/`, `runtime/`, `frontend/`, `retroarch/`, `config/`, `state/`,
  `cache/`, `logs/`, `ssh/` の役割を固定する。
- [ ] stock `/mnt/SDCARD/miyoo/lib` に依存しない `plumos-env` を作る。
- [x] stock BusyBox の癖を避けるため、plumOS 用の静的 BusyBox を build/deploy する。
- [x] vfat で扱えるよう BusyBox applet を symlink ではなく wrapper script 化する。
- [ ] Debian に近い操作感のため `procps-ng`, `coreutils`, `util-linux` などを
  `plumos/gnu/bin` に追加する。
- [ ] `ps`, `top`, `df`, `free`, `tar`, `find`, `grep`, `sed`, `awk`, `ip`, `ss`,
  `lsof`, `strace` の互換性を確認する。
- [ ] 動的 link が必要な場合の dynamic linker/shared library 同梱方針を検証する。
- [ ] 実機上で plumOS runtime path だけを使う smoke test を行う。

## Phase 3 - Device Deployment Loop

- [x] Docker 内で build した成果物を A30 へ転送する deploy script を作る。
- [x] SSH/SCP/rsync 相当の転送方法を決める。
- [x] 転送後に実機で command を実行し、log を回収する helper を作る。
- [ ] build -> deploy -> run -> collect logs の一連の流れを 1 command にまとめる。
- [ ] SD カードだけで復旧できる rollback 手順を文書化する。

## Phase 4 - Boot Bootstrap

- [x] rollback 可能な `MainUI` wrapper を作る。
- [x] wrapper から `/mnt/SDCARD/plumos/bin/plumos-frontend` を起動できるようにする。
- [x] wrapper 起動失敗時に stock MainUI へ戻れる fallback を作る。
- [x] wrapper と plumOS frontend の log を `/mnt/SDCARD/plumos/logs` へ出す。
- [x] stock MainUI を残したまま plumOS frontend prototype を手動起動して検証する。
- [ ] A30 再起動後も復旧可能な状態を保てることを確認する。

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
- [x] 1000 dummy ROM files で text mode 初回表示時間を計測する。
- [x] SSH から確認できる text mode system list / ROM list prototype を作る。
- [x] START menu から settings/apps/reboot/shutdown へ辿る UI model を実装する。
- [x] TOP の system 選択中に SELECT で system default core を選ぶ UI を実装する。
- [x] ROM list の ROM 選択中に SELECT で per-ROM core override を選ぶ UI を実装する。
- [x] core 選択の優先順位を ROM override > system default > plumOS recommended > auto detect として実装する。
- [x] core 選択は core id だけでなく launch profile id として保存できるようにする。
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
- [x] theme、layout preset、frontend behavior を分離した plumOS theme model を設計する。
- [x] `/mnt/SDCARD/plumos/themes/<theme_id>/theme.json` の schema を定義する。
- [x] theme は色、font、背景、system icon、selection 表現、spacing、thumbnail frame、sound effect だけを扱うようにする。
- [x] button 操作、START menu、SELECT core menu、favorite、ROM scan、resume などの behavior は theme から変更できないようにする。
- [x] text mode は theme asset/font が壊れていても default font/color で必ず操作できる fallback を実装する。
- [x] stock theme 互換は初期仕様に含めず、必要になった場合だけ importer として検討する。
- [x] brightness、volume、Wi-Fi、keymap など A30 設定 UI の扱いを決める。
- [ ] A30 settings の write-enabled controls は backend 検証、backup、atomic write、`sync` 方針を固めてから実装する。
- [x] framebuffer/input/audio の最小 runtime probe binary を A30 上で動かす。
- [ ] plumOS 同梱 SDL2 の最小 linked/render test binary を A30 上で動かす。
- [ ] stock MainUI を停止または置き換えた状態で audio playback を再検証する。
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
- [ ] `plumos-joystickd` の virtual input device が RetroArch/SDL から見えるか検証する。
- [ ] `/dev/mem` ADC 経路は stock calibration/test 画面由来の可能性として優先度を下げ、必要になった場合のみ再調査する。
- [x] Function button で開く SAFE menu prototype を controller UI に実装する。
- [ ] 電源ボタン短押しの event code と stock 側の sleep/shutdown 介入を安全に確認する。
- [ ] Function button で safe shutdown/resume menu を出す代替仕様を RetroArch 実行中に検証する。
- [ ] plumOS frontend 常用起動時に stock `keymon` を残すか停止するか最終判断する。
- [ ] RetroArch 実行中に Function button または利用可能なら電源キーで Sleep/Shutdown/Cancel menu を表示する方法を調査する。

## Phase 7 - RetroArch and Core Runtime

- [ ] RetroArch `v1.22.2` を A30 の armv7 hard-float 環境向けに build/test する。
- [ ] RetroArch を `/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ配置する。
- [ ] core を `/mnt/SDCARD/plumos/retroarch/cores` へ配置する。
- [ ] `HOME=/mnt/SDCARD/RetroArch` に依存しない起動方法へ移行する。
- [ ] `--config` と override config で system/core ごとの差分を管理する。
- [ ] libretro core は system ごとに段階更新する。
- [ ] 起動、performance、save/state、input、audio/video を core ごとに検証する。
- [ ] RetroArch の Auto Save State / Auto Load State を plumOS resume flow で検証する。
- [ ] Network Control Interface または同等手段で `SAVE_STATE`、`CLOSE_CONTENT`、`QUIT` を安全に実行できるか確認する。
- [ ] shutdown 前に save state、save RAM flush、RetroArch 終了、`sync` を順に実行する安全終了 script を作る。
- [ ] A30 で本物の suspend/resume が可能か確認し、難しい場合は疑似 sleep 方針を検討する。

## Phase 8 - A30 System Policy Validation

- [ ] CPU governor、CPU online/offline、clock policy を計測する。
- [ ] stock script の CPU2/CPU3 offline が妥当か判断する。
- [ ] `performance`, `ondemand`, `interactive` governor を比較する。
- [ ] game 終了後に CPU 状態を確実に戻す仕組みを作る。
- [ ] Wi-Fi の power sequence を plumOS 側で安全に再現できるか確認する。
- [ ] stock Wi-Fi userland を使い続けるか、plumOS 同梱へ移すか判断する。
- [ ] SSH を開発用 package のままにするか、plumOS service にするか決める。

## Phase 9 - Packaging and Release

- [ ] 開発用 Docker 関連ファイルも成果物/配布物の一部として扱う。
- [ ] SD カードへ展開する runtime package を作る。
- [ ] 開発者向け Docker/toolchain package を作る。
- [ ] end-user 向け release と developer 向け release の分け方を決める。
- [ ] license notice と upstream attribution を整理する。
- [ ] GitHub Release 用の package/artifact workflow を作る。
- [ ] fresh SD card 相当の環境で install/rollback を検証する。
