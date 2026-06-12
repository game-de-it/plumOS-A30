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
  こちらで管理する前提にします。libretro core は Onion 採用 core の実績 commit/build
  recipe を優先し、Onion に無い plumOS 独自 core は upstream latest/HEAD 候補として扱います。

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
- StockOS の `sysntpd`、`adbd`、`mtp` は rootfs 側 rc.d で起動されますが、plumOS
  boot wrapper が `plumos-stock-services boot` を呼んで停止します。rootfs の init script
  自体は変更しません。

## Wi-Fi

- service: `/etc/init.d/wpa_supplicant`
- 起動順: `S96wpa_supplicant`
- init script は `devmem` で Wi-Fi 電源系を操作し、`wlan0` を up してから以下を実行

```sh
wpa_supplicant -B -D nl80211 -iwlan0 -c /config/wpa_supplicant.conf
```

- `/etc/init.d/wpa_supplicant` 自体は DHCP を開始しない。IP取得は stock MainUI などの後段処理が
  `udhcpc -i wlan0` を実行している可能性が高い
- 状態ログは `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, `/proc/net/wireless`。stock MainUI なしでは
  `/tmp/wpa_status.txt` が存在しない場合がある
- stock `MainUI.stock` binary 文字列には `udhcpc -i wlan0 &`、
  `wpa_cli status > /tmp/wpa_status.txt`、`wpa_cli signal_poll >> /tmp/wpa_status.txt` が
  含まれる。stock UI は DHCP と runtime status 更新も担当している可能性が高い
- 2026-06-07 の poweroff -> 電源ON確認では、wrapper が boot uptime 約8秒で
  `plumos-network-rescue` を自動開始した。開始時は `wpa_supplicant` は起動済み、
  IPなし、`/proc/net/wireless` は未関連付け状態だった。DHCP attempt 1 が uptime 約9秒で
  始まり、約16秒で `192.168.10.165` の lease と Dropbear 起動まで完了した
- 2026-06-10 に StockOS `sysntpd` を停止し、DHCP 後に plumOS 管理の
  `/usr/sbin/ntpd -n -N -p time.cloudflare.com -p pool.ntp.org -p time.google.com -p time.apple.com`
  を起動するよう変更した。実機では約9時間進んでいたUTC時刻が `ntpd` により補正された。
- 2026-06-10 に FE/boot wrapper からの Network Recovery 導線は停止した。SSH がない状態での
  復旧は、StockOS MainUI から直接起動できる独立した shell script として別途設計する。
- 2026-06-10 に、Network Recovery 停止後も boot 時の自動接続を維持するため、
  wrapper が `plumos-network-control --wifi on` を background 実行するようにした。
  2026-06-13 以降、この処理は既存の `wlan0` IP がある場合は接続を落とさず維持し、
  IP が無い場合だけ保存済み `/config/wpa_supplicant.conf` から `wpa_supplicant`/DHCP
  を起動し、SSH helper を起動する。

実装上の意味:

- Wi-Fi の kernel module や電源投入手順は stock 環境に依存している可能性があります。
- まずは既存の電源投入手順を再現し、plumOS FE 起動時に DHCP retry を行います。そのうえで
  `wpa_supplicant`/DHCP client を plumOS 側へ同梱できるか検証します。
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

CPU policy 実測:

- governor 候補は `interactive`, `conservative`, `userspace`, `ondemand`, `performance`。
- cpufreq 側で 648000/816000/1200000/1344000 kHz などを設定できる。
- stock script は軽い機種で 648/816 MHz、重い機種で 1200 MHz 以上に寄せる傾向がある。
- plumOS RetroArch 実用 probe では `performance` は画面/音/操作が良好だったが、
  `ondemand` は音途切れが発生した。
- `userspace` + min/max/set speed を 648000 kHz に固定した NES/GB probe は
  画面/音/操作が良好だったため、軽量 core は fixed frequency を初期方針にする。
- 2026-06-07 にバッテリー駆動で dummy CPU負荷を測定した。
  2コア + performance + max load 120秒の平均は `current_now=487522`, `power=1977.781mW`。
  4コア + performance + max load 120秒の平均は `current_now=2020909`, `power=8066.453mW`。
  4コアは約4.1倍の平均電力になったため、core数もFEの設定対象にする。
- 同測定中、4コア側は途中で cpufreq min/max/current が 1344000 kHz から 1200000 kHz へ
  落ちた。電源/熱/PMIC側の上限制御の可能性がある。
- FE の SELECT core menu では system/ROM ごとに CPU frequency/core preset を見せ、
  実起動時は `plumos-retroarch-launch --cpu fixed --freq KHZ --cores 2|4` へ解決する。

詳細な UI 方針は [A30 設定 UI 方針](a30-settings-policy.md) に分離しています。

## Runtime probe

2026-06-06 に `plumos-runtime-probe` を A30 上で実行し、以下を確認しました。

- video: `/dev/fb0` は `480x640`, `32bpp`, line length `1920`
- video write: 64x64 の小さい patch を短時間描画し、復元できる
- input: `gpio-keys-polled` は `/dev/input/event3` として open/poll できる
- audio: `/dev/dsp` は存在するが、stock `MainUI` が `/dev/snd/pcmC0D0p` を保持している間は busy
- SDL2: plumOS 同梱 upstream SDL3 3.4.10 + sdl2-compat 2.32.68 と bundled
  dynamic loader/shared libraries で linked/window/input probe が起動し、composite
  virtual pad を GameController として自動認識できる
- SDL2 render: A30 は `/dev/fb*`, `/dev/mali`, `/dev/disp` を持つが `/dev/dri` は無い。
  upstream SDL3+sdl2-compat では `dummy`/`offscreen`/`evdev` の software renderer までで、
  実画面に出る SDL framebuffer/render backend は確認できない
- stock SDL2 video: stock `libSDL2-2.0.so.0.2600.1` は SDL `2.26.1` で、
  custom `mali` video driver と `offscreen` の2 driver 構成。`mali` は
  `/dev/fb0` + `/usr/lib/libMali.so` の fbdev EGL 経路で `opengles2` renderer を作る
- Mali EGL presenter: stock SDL にリンクしない `plumos-mali-egl-probe` で
  `/usr/lib/libEGL.so` と `/usr/lib/libGLESv2.so` を `dlopen` し、
  `eglCreateWindowSurface(..., NULL, ...)`, `eglMakeCurrent`,
  `eglSwapBuffers`, `glReadPixels` まで成功した

詳細は [A30 runtime probe](a30-runtime-probe.md) と
[A30 stock SDL 画面出力経路](a30-stock-sdl-video.md) にまとめています。

## Input policy

2026-06-06 に `plumos-input-compare` を A30 上で実行し、以下を確認しました。

- stock `keymon` は `event0`, `event1`, `event2`, `event3` を開いている
- stock `MainUI` も `event0`, `event1`, `event3` を開いている
- `gpio-keys-polled` は `/dev/input/event3`
- `keymon` と `MainUI` が動作中でも plumOS は `/dev/input/event3` を直接 open/poll できる
- 物理ボタン mapping を確認済み:
  A=`KEY_SPACE`, B=`KEY_LEFTCTRL`, START=`KEY_ENTER`, SELECT=`KEY_RIGHTCTRL`,
  Function=`KEY_ESC`
- 電源ボタン短押しは `/dev/input/event0` (`axp22-supplyer`) から `KEY_POWER`
  (`116`) として読める。plumOS 常用 FE 起動中の短押しでは stock sleep/shutdown
  介入は観測していない
- 左スティック軸は `/dev/input/event*` の `EV_ABS` としては観測できず、MainUI の
  calibration が `/config/joypad.config` を更新することを確認した
- spruceOS の A30 実装は `/dev/ttyS2` raw serial を `joystickinput` で virtual input
  に変換する構成で、stock A30 では `/dev/ttyS2` node は未作成だが `ttyS` minor 0-4
  は driver 上利用可能
- 2026-06-06 の実機では `/dev/ttyS0` から 9600/8N1 の `ff b1 b2 b3 b4 fe`
  joystick frame を確認した
- `plumos-joystickd` の最小実装で `/dev/uinput` から `plumOS A30 Analog Stick`
  (`js0`/`event4`) を作成できることを確認した
- `plumos-joystick-reader` で、`plumos-joystickd` が作る `js0`/`event4` を
  Linux joystick API と evdev の両方から読めることを確認した
- 左スティック押し込みは通常の kernel input event と `/dev/ttyS0` の6バイト
  serial frame では観測できなかった
- stockOS/spruceOS の analog stick calibration 保存内容は X/Y 軸の
  min/max/center のみで、押し込み設定は見当たらない。初期 plumOS では
  左スティック押し込みを未接続/未対応として扱う
- stock RetroArch は SDL1 有効、SDL2/udev/evdev 無効。`plumos-joystickd` の
  axes-only `js0` と composite `plumOS A30 Gamepad` は Linux joystick API では
  見えるが、stock RetroArch log では autoconfig/接続を確認できなかった
- MainUI から起動した stock RetroArch の Port1 Controls bind 待ち受けでは
  左スティックが `Axis -2`/`±2` として検出されるが、実際のメニューカーソル移動には
  使えていない
- stock PPSSPP は `launch.sh` から `miyoo282_xpad_inputd` を起動し、
  `/dev/ttyS0` と `/config/joypad.config` を使って `/dev/uinput` に
  `MIYOO Pad1` (`045e:028e`, `js0`/`event4`, 8 axes/11 buttons) を作る
- PPSSPP 本体は `libSDL2-2.0.so.0` と `SDL_GameController*` /
  `SDL_Joystick*` API で `MIYOO Pad1` を読んでおり、左スティック押し込みは
  PPSSPP の controller 設定でも反応しなかった
- stock `miyoo282_xpad_inputd` を起動せずに PPSSPP を直接起動した場合でも、
  `plumos-joystickd --device-mode xbox` の `plumOS A30 Gamepad` は
  SDL2 GameController mapping 成功で pad 1 に割り当てられた
- RetroArch/standalone emulator 向け analog 方針は、stock SDL1 依存ではなく
  `plumos-joystickd` の buttons+axes composite virtual pad mode と
  plumOS RetroArch の SDL2/evdev 対応を優先して検証する
- stock MainUI/keymon、PPSSPP direct launch、stock RetroArch probe の短時間確認後に
  `plumos-joystickd --device-mode xbox` の process と virtual device node は残らなかった。
  ただし 2026-06-07 の `/proc` 確認では、stock `keymon` が削除済みの過去
  `js*`/`event*` fd を保持する場合が見えたため、常駐設計前に fd 残りの影響を
  再検証する
- `plumos-joystickd --device-mode xbox` の button forwarding で A/B/X/Y、D-pad、
  L/R、L2/R2、START/SELECT、Function が `plumOS A30 Gamepad` 側の
  button/hat/trigger event として出ることを確認した
- plumOS 同梱 upstream SDL3 3.4.10 + sdl2-compat 2.32.68 の probe でも、
  `plumos-joystickd --device-mode xbox` の composite virtual pad は
  `Xbox 360 Controller` mapping として自動認識された
- RetroArch 実行中の safe shutdown trigger は電源ボタン短押しへ寄せる。
  Function button は standalone emulator の menu 操作と競合させない

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

RetroArch 本体は build 時点で公式 upstream の latest stable release を再確認してから
採用候補を決めます。A30 は glibc `2.23` のため、汎用 buildbot binary に頼らず、
A30 向け armv7 hard-float build を作る前提にします。libretro core は Onion 採用 core の
実績 commit/build recipe を優先し、Onion に無い plumOS 独自 core は upstream latest/HEAD
候補として扱います。stockOS 側の RetroArch/core version は互換調査の参考に留め、
実機異常が出た場合だけ比較対象にします。

2026-06-07 時点では、plumOS 側で RetroArch 1.22.2 minimal display probe を
`/mnt/SDCARD/plumos/retroarch/bin/retroarch` へ配置し、A30 実機で
`mali_fbdev` + GLES/EGL + RGUI 表示を確認済みです。A30 の物理画面で横向きにするには、
RetroArch の `video_rotation` だけでは足りず、GL2 menu/default draw の MVP を A30 の
`fbdev_mali` context だけ rotation 済み `mvp` へ切り替える patch と `--rotation ccw` を
使います。この minimal build は表示確認用で、core/audio/input は full runtime 段階で
検証します。

2026-06-12 時点の RetroArch practical build では、上記の `video_rotation=1` / GL2 MVP
回転方式ではなく、A30 専用の landscape FBO present を既定にします。RetroArch には
logical screen を 640x480 として認識させ、Mali fbdev native surface は物理 480x640 の
まま使います。描画は 640x480 FBO に集約し、最後の present だけ90度回転するため、
FPS OSD や RGUI text の横引き伸ばしを避け、WonderSwan など core 側 rotation とも
矛盾しにくい構成になります。

## 既存 launch script の傾向

多くの emulator script は以下を行います。

- CPU governor を `performance` にする
- CPU2/CPU3 を offline にすることがある
- system ごとの `overclock` helper に `648`, `816`, `1200`, `1512` などを渡す
- `HOME=/mnt/SDCARD/RetroArch/` を指定して RetroArch を起動
- `retroarch -v -L CORE_PATH ROM_PATH` または
  `ra32.miyoo -v -L CORE_PATH ROM_PATH` を実行

例外:

- PPSSPP は standalone の `PPSSPPSDL` と `miyoo282_xpad_inputd` を使い、
  Xbox 360 互換風の `MIYOO Pad1` virtual pad を SDL2 経由で読む
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
- build 時点で最新 stable の RetroArch を確認し、A30 向けに build する。
  RetroArch 本体の minimal RGUI 表示は 1.22.2 で確認済み
- libretro core は Onion 採用 core と plumOS 独自 core の union recipe から A30 向けに
  再 build する。Onion 側の実績 commit/build recipe がある core はそれを優先する
- core は一括更新せず、system ごとに起動、performance、save/state、input を検証する
- CPU policy、Wi-Fi、input、audio/video driver は「既存仕様を疑う」対象として扱う
