# TODO

## 完了

- SD カードから起動できる開発用 SSH kit を作成する
- 作業用 PC の公開鍵を `plumos/ssh/etc/authorized_keys` に置けるようにする
- A30 の Ports から `Start SSH` を実行して SSH 接続できるようにする
- `scripts/collect-a30-info.sh root@A30_IP_ADDRESS` で実機情報を収集する
- 起動フロー、mount、Wi-Fi、stock frontend、RetroArch 構成を文書化する

## 次の実装/調査

- rollback 可能な `MainUI` wrapper を作る
- wrapper から `/mnt/SDCARD/plumos/bin/plumos-frontend` を起動できるようにする
- `/mnt/SDCARD/plumos` 配下の runtime directory 構成を package 化する
- stock `/mnt/SDCARD/miyoo/lib` に依存しない `plumos-env` を作る
- stock MainUI を残したまま、plumOS frontend prototype を手動起動して検証する
- `Emu`, `RApp`, `App`, `Themes`, `Roms`, `Imgs` の stock JSON schema を読む
  compatibility layer を作る
- 既存 ROM/artwork/save/state を移動せずに認識できるようにする
- launch 互換として `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path `$1`, `launchlist`,
  `recentlist.json` の扱いを再現する
- SDL/input/audio/video の最小 test binary を A30 上で動かす
- stock `keymon` を残す場合と直接 `/dev/input/event*` を読む場合を比較する
- A30 向け sysroot/toolchain を再現可能な形で用意する
- RetroArch `v1.22.2` を A30 の armv7 hard-float 環境向けに build/test する
- libretro core は system ごとに段階更新し、起動、performance、save/state、input を
  確認する
- CPU governor、CPU online/offline、clock policy を計測して、stock script の挙動が
  妥当か判断する
- Wi-Fi の power sequence を plumOS 側で安全に再現できるか確認する
- SSH を開発用 package のままにするか、plumOS service にするか決める
- GitHub release 用の package/artifact workflow を決める
