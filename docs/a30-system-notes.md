# Miyoo A30 システム調査メモ

2026-06-06 に実機へ SSH 接続して収集した情報です。生ログは `artifacts/` 配下に
保存していますが、ローカルの Wi-Fi 設定を含む可能性があるため git 管理外にして
います。

## ハードウェアと OS

- kernel: Linux `3.4.39`, `armv7l`, `sun8i`
- CPU: ARMv7, 4 cores, NEON/VFPv4 対応
- RAM: 512 MB
- firmware base: Tina/OpenWrt 系、`Neptune 3.5.1`
- target: `r16-parrot/generic v2.2`
- libc: glibc `2.23`
- 表示系ログでは `480x640` framebuffer と Mali/GLES の利用を確認

実装上の意味:

- A30 は古い 32-bit ARM/glibc 環境なので、現在の一般的な Linux armhf バイナリが
  そのまま動くとは限りません。
- RetroArch やライブラリを最新版へ更新する場合、A30 向けの sysroot/ランタイムを
  こちらで管理する前提にします。

## ストレージとマウント

- rootfs: `/dev/mtdblock3` が `/` に `squashfs` read-only でマウント
- 設定領域: `/dev/mtdblock2` が `/config` に JFFS2 read-write でマウント
- SD カード: `/dev/mmcblk0p1` が `/mnt/SDCARD` に vfat でマウント
- ランタイム領域: `/tmp`, `/run`, `/dev` は tmpfs
- `/var` は `/tmp` へのリンク

実装上の意味:

- 初期段階では rootfs/NAND を書き換えず、SD カードベースで進めます。
- plumOS の実体は `/mnt/SDCARD/plumos` 配下へ集約します。
- 既存起動処理との接点として、最小限の bootstrap だけを既存の起動パスへ置く可能性
  があります。

## 起動フロー

確認できた起動順序:

1. kernel command line は `root=/dev/mtdblock3`, `rootfstype=squashfs`,
   `init=/sbin/init`, `rdinit=/rdinit` を指定
2. PID 1 は `/sbin/procd`
3. `/etc/inittab` が `/etc/init.d/rcS S boot` を実行
4. OpenWrt 風の `rc.d` が `network`, `wpa_supplicant`, `sysntpd`, `adbd`,
   `mtp`, `done` などを起動
5. `/etc/init.d/done` が `/etc/rc.local` を実行
6. `/etc/rc.local` は出力を `/tmp/rc.local.log` に流し、`/etc/main &` を起動
7. `/etc/main` が Miyoo 用の環境変数、音量、`/config`、更新処理、`keymon`、
   フロントエンド起動を担当

`/etc/main` のフロントエンド起動優先度:

- `/mnt/SDCARD/miyoo/` が存在する場合
  - `LD_LIBRARY_PATH=/lib:/usr/lib:/mnt/SDCARD/miyoo/lib`
  - `/mnt/SDCARD/miyoo/app/keymon` を起動
  - `/mnt/SDCARD/miyoo/app` へ移動
  - `./MainUI` を実行
- SD 側の `miyoo/` が存在しない場合
  - `/usr/miyoo/bin/keymon` と `/usr/miyoo/bin/MainUI` へ fallback

実装上の意味:

- 最も安全な差し替え地点は SD カード側の `/mnt/SDCARD/miyoo/app/MainUI` です。
- ただし plumOS の本体はここへ置かず、`MainUI` は
  `/mnt/SDCARD/plumos/bin/plumos-frontend` などを起動する薄いラッパーにします。
- rootfs 書き換えは、起動フローと復旧手段が十分に固まるまで後回しにします。

## Wi-Fi

- service: `/etc/init.d/wpa_supplicant`
- 起動順: `S96wpa_supplicant`
- init script は `devmem` で Wi-Fi 電源系を操作し、`wlan0` を up してから以下を実行

```sh
wpa_supplicant -B -D nl80211 -iwlan0 -c /config/wpa_supplicant.conf
```

- DHCP は `udhcpc -i wlan0`
- 状態ログは `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, `/proc/net/wireless`

実装上の意味:

- Wi-Fi の kernel module や電源投入手順は stock 環境に依存している可能性があります。
- まずは既存の電源投入手順を再現し、そのうえで plumOS 側に
  `wpa_supplicant`/DHCP client を同梱できるか検証します。
- `/config/wpa_supplicant.conf` は実機固有の機微情報を含むため、git には入れません。

## 現在のフロントエンド

- 実行中の binary: `/mnt/SDCARD/miyoo/app/MainUI`
- working directory: `/mnt/SDCARD/miyoo/app`
- companion process: `/mnt/SDCARD/miyoo/app/keymon`
- 観測時の footprint: 約 21 MB RSS、12 threads
- user/system settings: `/config/system.json`
- theme font/color settings: `/mnt/SDCARD/miyoo/res/config.json`
- platform/gamelist mapping: `/mnt/SDCARD/miyoo/app/oplatform.json`
- recent list: `/mnt/SDCARD/Roms/recentlist.json`

`/config/system.json` で確認した主な設定:

- 音量、mute、BGM 音量
- 輝度、画面色調整
- language
- theme path
- keymap
- Wi-Fi enabled flag
- CPU frequency mode flag

2026-06-06 に確認した現在値:

- `vol=14`, `mute=1`, `bgmvol=13`
- `brightness=10`, `lumination=5`
- `contrast=10`, `hue=10`, `saturation=10`
- `wifi=1`, `cpufreq=0`
- `keymap=L,L2,R,R2,X,A,B,Y`
- `language=en.lang`

backend 候補:

- volume: `amixer` は存在するが mixer control mapping は未検証
- brightness: `/sys/class/backlight` には利用できる brightness file が見当たらない
- Wi-Fi runtime: `/tmp/wpa_status.txt` から redacted status は読める
- input: `gpio-keys-polled` は通常 `/dev/input/event3`

詳細な UI 方針は [A30 設定 UI 方針](a30-settings-policy.md) に分離しています。

## Runtime probe

2026-06-06 に `plumos-runtime-probe` を A30 上で実行し、以下を確認しました。

- video: `/dev/fb0` は `480x640`, `32bpp`, line length `1920`
- video write: 64x64 の小さい patch を短時間描画し、復元できる
- input: `gpio-keys-polled` は `/dev/input/event3` として open/poll できる
- audio: `/dev/dsp` は存在するが、stock `MainUI` が `/dev/snd/pcmC0D0p` を保持している間は busy
- SDL2: stock library は存在するが、plumOS の runtime dependency としてはまだ採用しない

詳細は [A30 runtime probe](a30-runtime-probe.md) にまとめています。

## Input policy

2026-06-06 に `plumos-input-compare` を A30 上で実行し、以下を確認しました。

- stock `keymon` は `event0`, `event1`, `event2`, `event3` を開いている
- stock `MainUI` も `event0`, `event1`, `event3` を開いている
- `gpio-keys-polled` は `/dev/input/event3`
- `keymon` と `MainUI` が動作中でも plumOS は `/dev/input/event3` を直接 open/poll できる
- 電源ボタン以外の物理ボタン mapping を確認済み:
  A=`KEY_SPACE`, B=`KEY_LEFTCTRL`, START=`KEY_ENTER`, SELECT=`KEY_RIGHTCTRL`,
  Function=`KEY_ESC`

当面は stock `keymon` を残しつつ、plumOS frontend は `/dev/input/event3` を直接読む方針です。
詳細は [A30 input policy](a30-input-policy.md) に分離しています。

## 継承すべきフロントエンド互換情報

stock UI は SD カード上の以下を読みます。

- `/mnt/SDCARD/Emu/*/config.json`: 主要 emulator category
- `/mnt/SDCARD/RApp/*/config.json`: 追加/代替 libretro core
- `/mnt/SDCARD/App/*/config.json`: app/tool
- `/mnt/SDCARD/Themes/*/config.json`: theme
- `/mnt/SDCARD/Roms/*`: ROM folder
- `/mnt/SDCARD/Imgs/*`: artwork folder

確認した config field:

- `label`
- `icon`
- `iconsel`
- `launch`
- `rompath`
- `imgpath`
- `gamelist`
- `useswap`
- `shortname`
- `hidebios`
- `extlist`
- `launchlist`
- `description`

互換維持に必要なこと:

- system/app/theme を hard-code せず JSON から列挙する
- `extlist` で ROM を filter する
- `launchlist` の代替 launcher/core を扱う
- 選択した ROM path を launch script の `$1` として渡す
- launch 前に `EMU_DIR` を emulator directory へ設定する
- `Roms/recentlist.json` の更新を維持する
- `imgpath` による artwork lookup を維持する
- theme discovery と font/color metadata を維持する

## RetroArch と core

- 現在の RetroArch version: `1.16.0`
- `retroarch` build string: `MIYOO Build: Apr 25 2024 22:32:35`
- `ra32.miyoo` build string: `MIYOO Build: Apr 4 2024 15:25:23`
- どちらも 32-bit binary
- 既存構成では `/mnt/SDCARD/miyoo/lib` の SDL 系 library に依存
- core directory: `/mnt/SDCARD/RetroArch/.retroarch/cores`
- installed core `.so`: 73 個
- core directory size: 約 908 MB

既存 `retroarch.cfg` の主な設定:

- `audio_driver = "sdl"`
- `input_driver = "sdl"`
- `input_joypad_driver = "sdl"`
- `libretro_directory = "./.retroarch/cores"`
- `system_directory = "./.retroarch/system"`
- `savefile_directory = "./.retroarch/saves"`
- `savestate_directory = "./.retroarch/states"`

2026-06-06 時点で公式 upstream の GitHub Releases では RetroArch `v1.22.2` が
最新として表示されています。A30 は glibc `2.23` のため、汎用 buildbot binary に
頼らず、A30 向け armv7 hard-float build を作る前提にします。

## 既存 launch script の傾向

多くの emulator script は以下を行います。

- CPU governor を `performance` にする
- CPU2/CPU3 を offline にすることがある
- system ごとの `overclock` helper に `648`, `816`, `1200`, `1512` などを渡す
- `HOME=/mnt/SDCARD/RetroArch/` を指定して RetroArch を起動
- `retroarch -v -L CORE_PATH ROM_PATH` または
  `ra32.miyoo -v -L CORE_PATH ROM_PATH` を実行

例外:

- PPSSPP は standalone の `PPSSPPSDL` と input daemon を使う
- arcade/shooting 系では `ra32.miyoo` と `"$*"` を使う path がある
- PS 系 script は game ごとの BIOS または default BIOS を RetroArch system path に
  copy することがある

## 既存仕様の妥当性評価

既存の起動方法は「動いている」という点では重要ですが、plumOS でそのまま踏襲する
必要はありません。現時点で疑って検証したい点は以下です。

- launch script が system ごとに散らばっており、環境変数や CPU 設定の復元が曖昧
- `HOME` と相対 path に依存しているため、構成変更時に壊れやすい
- CPU2/CPU3 を offline にする判断が妥当か未検証
- `overclock` 値が performance、発熱、battery、stability に対して最適か未検証
- `keymon` が本当に必要か、または SDL/input event を直接扱えるか未検証
- Wi-Fi の userland を stock に任せるべきか、plumOS 同梱へ移せるか未検証

plumOS では、まず既存挙動を再現できる互換 layer を作り、その後に計測しながら
より良い方式へ置き換えます。

## 次の判断

- `/mnt/SDCARD/miyoo/app/MainUI` は復旧可能な wrapper として扱う
- plumOS 本体は `/mnt/SDCARD/plumos` 配下へ置く
- stock の `/mnt/SDCARD/miyoo/lib` には依存しない
- RetroArch `v1.22.2` と必要 core を A30 向けに build する
- core は一括更新せず、system ごとに起動、performance、save/state、input を検証する
- CPU policy、Wi-Fi、input、audio/video driver は「既存仕様を疑う」対象として扱う
