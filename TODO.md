# TODO

このファイルは現在の作業対象だけを置く。作業履歴、検証ログ、判断理由は `AGENTS.md` の方針に従い、`docs/`、commit log、`artifacts/` に残す。`HANDOFF.md` には追記しない。

## Phase 0 - Repository and Remote Access

- [x] `/Users/kroot/plumOS-A30` に local git repository を用意する。
- [x] SD カード起動の開発用 SSH kit を用意する。
- [x] A30 へ SSH 接続して情報収集できるようにする。
- [x] 起動フロー、mount、Wi-Fi、stock frontend、RetroArch 構成を文書化する。
- [x] 日本語を正本、英語を `.en.md` とするドキュメント構成にする。
- [ ] GitHub 公開前に remote、license、release 方針を確定する。

## Phase 1 - Docker Toolchain and Build Environment

- [x] plumOS 専用 Docker build environment を用意する。
- [x] frontend/helper/RetroArch/libretro core/standalone emulator を Docker 内で build できるようにする。
- [x] Docker build output を `dist/` または staging directory へ集約する。
- [x] build cache と大きな生成物が git に入らないよう整理する。
- [x] SDL2 runtime と RetroArch minimal build の upstream version/build option を manifest 化する。
- [ ] A30 向け sysroot を Docker build で再現可能にする。
- [ ] emulator/core/helper library の採用 version/tag/commit/build option を manifest 化する。

## Phase 2 - plumOS Runtime Layout

- [ ] `/mnt/SDCARD/plumos` 配下の runtime directory 構成を package 化する。
- [x] `config/frontend`, `config/system`, `config/network`, `config/performance` の責務を分ける。
- [x] stock `/mnt/SDCARD/miyoo/lib` に依存しない `plumos-env` を用意する。
- [x] plumOS 用 static BusyBox と vfat-safe wrapper applets を用意する。
- [x] `ip`, `ss`, `strace` など補助 userland tool の first-pass を追加する。
- [x] `ps`, `top`, `df`, `free`, `tar`, `find`, `grep`, `sed`, `awk`, `ip`, `ss`, `lsof`, `strace` の実機 smoke を行う。
- [x] dynamic linker/shared library 同梱方針を SDL2 probe で検証する。
- [x] plumOS runtime path だけを使う smoke test を行う。
- [ ] 必要に応じて `procps-ng`, `coreutils`, `util-linux` などを追加する。

## Phase 3 - Device Deployment Loop

- [x] Docker 成果物を A30 へ転送する deploy script を作る。
- [x] SSH/SCP/rsync 相当の転送方法を決める。
- [x] 実機 command 実行と log 回収 helper を作る。
- [x] `/dev/fb0` capture helper を作る。
- [x] display/emulator process 停止手順を `scripts/stop-a30-display-processes.sh` に固定する。
- [ ] build -> deploy -> run -> collect logs を 1 command にまとめる。
- [ ] SD カードだけで復旧できる rollback 手順を文書化する。

## Phase 4 - Boot Bootstrap

- [x] rollback 可能な `MainUI` wrapper を作る。
- [x] wrapper から `plumos-controller-ui-mali` を通常 FE として起動する。
- [x] wrapper 起動失敗時に stock MainUI へ戻れる fallback を作る。
- [x] wrapper と plumOS frontend の log を `/mnt/SDCARD/plumos/logs` へ出す。
- [x] boot 時に stock `keymon` を止め、plumOS FE と network recovery path を起動する。
- [x] START menu と Network Settings から network recovery を実行できるようにする。
- [x] A30 の電源再投入後に Wi-Fi/DHCP/SSH/FE が復旧できることを確認する。
- [ ] stock MainUI に reboot 項目がないため、安全な OS 再起動可否を決める。

## Phase 5 - Frontend Compatibility Layer

- [x] stock `Emu`, `RApp`, `App`, `Themes` の `config.json` を inventory として読む。
- [x] `rompath`, `imgpath`, `gamelist`, `launchlist` を解決して存在と件数を確認する。
- [x] ROM file と artwork file の名前対応 rule を確認する。
- [x] compatibility scanner で `extlist`, `launchlist`, `recentlist` の存在を検出する。
- [x] stock 仕様流用時の decision record template を作る。
- [ ] `gamelist` XML の stock data schema を読む。
- [ ] 既存 ROM、artwork、save、state を移動せず認識できるようにする。
- [ ] frontend ROM list で `extlist` filter を実装する。
- [ ] `launchlist` の代替 launcher/core を UI と launch profile で扱えるようにする。
- [ ] `Roms/recentlist.json` 互換の recent list を parse/update できるようにする。
- [ ] `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path `$1` の launch 互換を再現する。
- [ ] shell launch script 依存を plumOS launch profile へ移す。

## Phase 6 - Frontend Implementation

- [x] plumOS frontend 独自の system/app/theme data model を設計する。
- [x] `systems.json` seed、ROM scanner、on-enter scan、directory game scan を実装する。
- [x] text UI の system/ROM/list/start/settings/core/favorites/recent/resume を実装する。
- [x] controller UI の TOP/ROM/START/Settings/SAFE/HELP/Core select を実装する。
- [x] Mali TTY renderer、rotation auto、A30 compact layout、blue/orange accent を実装する。
- [x] TOP/ROM list の paging、scroll、key repeat、日本語表示、marquee を実装する。
- [x] START menu と settings 階層を UI Settings/System/Network/Performance/Apps/HELP/Shutdown に整理する。
- [x] UI Settings の checkbox/value selector と保存反映を実装する。
- [x] System Settings の Volume/Brightness/Lumination/Display Color/Language を plumOS config と runtime backend に接続する。
- [x] Brightness は `1..20` を保存し、RAW lcdbl table `2,3,4,5,6,7,8,9,10,26,43,59,75,92,108,125,141,157,174,190` に割り当てる。
- [x] 検証用 brightness tile screen を通常ユーザー操作から隠し、`PLUMOS_CONTROLLER_BRIGHTNESS_TEST=1` の debug 機能にする。
- [x] Network Settings の read-only status と network recovery action を実装する。
- [x] Network Settings の FTP/SFTP/Samba checkbox と永続 ON/OFF を実装し、実機 smoke を行う。
- [x] Performance Settings を既存 CPU profile/core override 機能へ接続する。
- [x] A30 UI design rules を `docs/a30-ui-design.md` にまとめる。
- [ ] Graphic UI mode / gallery mode を実装する。
- [ ] artwork scraper と thumbnail cache pipeline を設計する。
- [ ] ROM thumbnail scraping を実装する。
- [ ] 現行 TTY prompt の横圧縮表示を解消する。
- [ ] 物理音量ボタンと brightness hotkey を System Settings と連動する。
- [ ] UI 文言の language localization を実装する。
- [ ] Theme selection の候補名/path/適用範囲を決める。
- [x] Wi-Fi editor の backup/rollback 付き安全編集 flow を設計する。
- [x] Connect Wi-Fi の SSID 検索、password 入力、DHCP、gateway ping、IP 表示を実機で確認する。
- [x] FTP/SFTP/Samba の複数ファイル同時転送と速度を測定し、README の推奨サービスを更新する。
- [x] USB Disk Mode 正式化前に `dosfsck` / FAT checker を plumOS userland へ同梱する。
- [x] USB Disk Mode の enter/finish flow を実機で検証し、通常UIへ出せる安全な復帰手順を固める。

## Phase 7 - RetroArch, Libretro, and Standalone Runtime

- [x] RetroArch minimal/practical runtime を A30 向けに build/deploy する。
- [x] RetroArch practical runtime で OSS audio、SDL2 joypad、FE-owned launch を確認する。
- [x] SELECT+B menu、SELECT+START quit、FE 復帰時の input drain/cooldown を実装する。
- [x] `fceumm` と `gambatte` を build/deploy し、NES/GB の実画面確認を行う。
- [x] Class A/B libretro core を A30 armv7 hard-float 向けに build する。
- [x] PPSSPP standalone の表示、入力、menu hotkey、config defaults、joystickd 起動を固める。
- [x] ScummVM、EasyRPG Player、PCSX-ReARMed、DOSBox-Pure の起動方針を固める。
- [x] standalone DOSBox Staging を通常配布対象から外し、DOSBox-Pure を DOS 標準候補にする。
- [x] FE-owned launcher の CPU restore、joystickd 停止、fb restore、FE 復帰を実装する。
- [ ] build 済み Class A/B core を system ごとに deploy/validate する。
- [ ] standalone launcher の通常終了/TERM/KILL/SSH 切断時 cleanup を再検証する。
- [ ] FE A ボタン経路で PPSSPP/PCSX/ScummVM/EasyRPG/RetroArch の通常終了をユーザー操作で確認する。
- [ ] PPSSPP config/controls の backup、restore、factory reset 機能を実装する。
- [ ] stock PPSSPP 横メニュー成立要因を公式 PPSSPP との差分として整理する。
- [ ] `--config` と override config で system/core ごとの差分を管理する。
- [ ] RetroArch Auto Save State / Auto Load State と plumOS resume flow を検証する。

## Phase 8 - A30 Input, Power, and System Policy

- [x] A30 物理入力 mapping と `/dev/input/event*` 経路を確認する。
- [x] `/dev/ttyS0` serial stick frame と `plumos-joystickd` を実装する。
- [x] `plumos-joystickd --device-mode xbox` を SDL2/PPSSPP/RetroArch 経路で確認する。
- [x] Function SAFE menu、safe shutdown、resume hold、mem sleep、poweroff を実装/確認する。
- [x] CPU governor/frequency/core policy を計測し、FE default を 648MHz / 2 cores にする。
- [x] Wi-Fi power sequence、DHCP、Dropbear 復旧を複数回確認する。
- [ ] 左スティック上下左右を個別 capture し、X/Y と符号を確定する。
- [ ] 電源ボタン短押し event code と stock 側 sleep/shutdown 介入を切り分ける。
- [ ] RetroArch 実行中の Function UX を overlay menu にするか direct safe exit にするか決める。
- [ ] stock Wi-Fi userland を使い続けるか、plumOS 同梱へ移すか判断する。
- [ ] SSH を開発用 package のままにするか、plumOS service にするか決める。

## Phase 9 - Packaging and Release

- [ ] SD カードへ展開する runtime package を作る。
- [ ] 開発者向け Docker/toolchain package を作る。
- [ ] end-user release と developer release の分け方を決める。
- [ ] StockOS と plumOS の差分を整理する。
- [ ] plumOS 導入メリットを end-user 向けにまとめる。
- [ ] StockOS から plumOS へ移行する際の制約/トレードオフを整理する。
- [ ] license notice と upstream attribution を整理する。
- [ ] GitHub Release 用 package/artifact workflow を作る。
- [ ] fresh SD card 相当の環境で install/rollback を検証する。
