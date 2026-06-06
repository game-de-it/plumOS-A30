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

- [ ] plumOS 専用の Docker build environment を設計する。
- [ ] `docker/` 配下に toolchain Dockerfile と補助 script を置く。
- [ ] A30 向け sysroot を Docker build で再現可能にする。
- [ ] Docker 内で frontend、helper、RetroArch、libretro core を build できるようにする。
- [ ] Docker build output を `dist/` または staging directory へ集約する。
- [ ] build cache と大きな生成物が git に入らないよう ignore/配置を整理する。
- [ ] Docker image の作り方と使い方を日本語/英語で文書化する。

## Phase 2 - plumOS Runtime Layout

- [ ] `/mnt/SDCARD/plumos` 配下の runtime directory 構成を package 化する。
- [ ] `bin/`, `lib/`, `runtime/`, `frontend/`, `retroarch/`, `config/`, `state/`,
  `cache/`, `logs/`, `ssh/` の役割を固定する。
- [ ] stock `/mnt/SDCARD/miyoo/lib` に依存しない `plumos-env` を作る。
- [ ] 動的 link が必要な場合の dynamic linker/shared library 同梱方針を検証する。
- [ ] 実機上で plumOS runtime path だけを使う smoke test を行う。

## Phase 3 - Device Deployment Loop

- [ ] Docker 内で build した成果物を A30 へ転送する deploy script を作る。
- [ ] SSH/SCP/rsync 相当の転送方法を決める。
- [ ] 転送後に実機で command を実行し、log を回収する helper を作る。
- [ ] build -> deploy -> run -> collect logs の一連の流れを 1 command にまとめる。
- [ ] SD カードだけで復旧できる rollback 手順を文書化する。

## Phase 4 - Boot Bootstrap

- [ ] rollback 可能な `MainUI` wrapper を作る。
- [ ] wrapper から `/mnt/SDCARD/plumos/bin/plumos-frontend` を起動できるようにする。
- [ ] wrapper 起動失敗時に stock MainUI へ戻れる fallback を作る。
- [ ] wrapper と plumOS frontend の log を `/mnt/SDCARD/plumos/logs` へ出す。
- [ ] stock MainUI を残したまま plumOS frontend prototype を手動起動して検証する。
- [ ] A30 再起動後も復旧可能な状態を保てることを確認する。

## Phase 5 - Frontend Compatibility Layer

- [ ] `Emu`, `RApp`, `App`, `Themes`, `Roms`, `Imgs` の stock JSON schema を読む。
- [ ] 既存 ROM、artwork、save、state を移動せずに認識できるようにする。
- [ ] `extlist` による ROM filter を実装する。
- [ ] `launchlist` の代替 launcher/core を扱えるようにする。
- [ ] `Roms/recentlist.json` 互換の recent list を扱う。
- [ ] `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path `$1` の launch 互換を再現する。
- [ ] 最終的には shell launch script 依存を plumOS launch profile へ移す。

## Phase 6 - Frontend Implementation

- [ ] controller-first の最小 frontend prototype を作る。
- [ ] system list、ROM list、recent、favorites、settings を実装する。
- [ ] theme、font、artwork の読み込み方針を決める。
- [ ] brightness、volume、Wi-Fi、keymap など A30 設定 UI の扱いを決める。
- [ ] SDL/input/audio/video の最小 test binary を A30 上で動かす。
- [ ] stock `keymon` を残す場合と直接 `/dev/input/event*` を読む場合を比較する。

## Phase 7 - RetroArch and Core Runtime

- [ ] RetroArch `v1.22.2` を A30 の armv7 hard-float 環境向けに build/test する。
- [ ] RetroArch を `/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ配置する。
- [ ] core を `/mnt/SDCARD/plumos/retroarch/cores` へ配置する。
- [ ] `HOME=/mnt/SDCARD/RetroArch` に依存しない起動方法へ移行する。
- [ ] `--config` と override config で system/core ごとの差分を管理する。
- [ ] libretro core は system ごとに段階更新する。
- [ ] 起動、performance、save/state、input、audio/video を core ごとに検証する。

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
