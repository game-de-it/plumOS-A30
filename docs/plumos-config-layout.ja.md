# plumOS 設定ファイル配置方針

plumOS に関する永続ファイルは `/mnt/SDCARD/plumos/` 配下に置きます。stockOS の
`/config` や stock frontend の設定ファイルは plumOS の通常設定として書き換えません。

## 基本方針

- 設定は巨大な `system.json` 1本に集約しない。
- UI、system、network、performance などの責務ごとに分ける。
- 設定ファイルは人が読める JSON にする。
- 保存は tmp file、`fsync`、atomic rename、`sync` を使う。
- 互換性のため、各ファイルに `version` を置く。
- 機微情報は通常の設定ファイルや git 管理対象に入れない。
- stockOS-owned な `/config/system.json` は参照・書き込み対象にしない。
- `deploy-a30.sh` は既存の mutable settings を package default で上書きしない。
  初回導入では default を配置し、既存ファイルがある場合は展開後に復元する。
  `config/*/settings.json` と RetroArch の主設定
  `retroarch/config/retroarch-minimal.cfg` / `retroarch/config/retroarch-practical.cfg`
  に加えて、standalone emulator の env override
  `config/standalone/` と PPSSPP runtime 設定
  `state/standalone/ppsspp/` は保護対象に含める。

## 現在の配置

- `/mnt/SDCARD/plumos/config/frontend/settings.json`
  - UI Mode、Graphic Theme、TOP/ROM list 表示、sort、scan、「起動時に前回ROMを開く」など frontend の挙動。
- `/mnt/SDCARD/plumos/config/frontend/menus.json`
  - START menu など、frontend menu 定義。
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - system 定義、ROM directory alias、launch profile、artwork lookup。
- `/mnt/SDCARD/plumos/config/system/settings.json`
  - plumOS-owned の本体設定。Volume、Brightness、Lumination、Display Color、Time Settings、Language。
- `/mnt/SDCARD/plumos/share/frontend/lang/*.lang`
  - frontend の UI 文言辞書。`language` 設定で選択し、`key=value` 形式で項目名や説明欄を
    翻訳する。辞書に存在しない key は英語 fallback を使う。
- `/mnt/SDCARD/plumos/factory-defaults/{ra,pico,sa}/`
  - RA、PicoArch、standalone emulator の出荷時設定として保存したファイル群。
  - path は `/mnt/SDCARD/plumos/` からの相対配置を保つ。例:
    `factory-defaults/ra/retroarch/home/.config/retroarch/retroarch.cfg`。
  - `plumos-factory-reset` はここに存在する file だけを復元対象にし、ROM、BIOS、
    save data、shader cache などは対象にしない。

## 今後の候補

- `/mnt/SDCARD/plumos/config/network/settings.json`
  - Wi-Fi enable 方針、network recovery 方針など。SSID/PSK は別管理にする。
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - 既存の system/ROM 別 launch profile、CPU policy/frequency/core override。
  - Performance Settings はこの既存機能へ接続済み。
- `/mnt/SDCARD/plumos/config/performance/profiles.json`
  - 将来、global preset や名前付き performance profile が必要になった場合に追加する。
- `/mnt/SDCARD/plumos/config/standalone/<emulator>.env`
  - standalone launcher の環境変数 override。PPSSPP の表示、入力、CPU など、実機で調整した
    起動条件は `plumos-standalone-launch` 本体ではなく `ppsspp.env` に置く。
  - PicoArch の launch 既定値は `picoarch.env` に置く。BIOS/system directory は
    `PLUMOS_PICOARCH_BIOS_DIR=/path` で指定できるが、PicoArch 側の core/ROM directory 別
    `picoarch.cfg` に `bios_dir = /path` がある場合はそちらが優先される。
  - PCSX-ReARMed standalone は `PLUMOS_A30_PSX_BIOS_DIR` を読み、既定値は
    `/mnt/SDCARD/Bios` とする。upstream standalone PCSX は通常 `$HOME/.pcsx/bios` を
    scan するため、plumOS 側で明示的に共有 BIOS root へ向ける。
  - `deploy-a30.sh` の保護対象なので standalone package を再 deploy しても維持する。
  - PPSSPP の `ppsspp.ini` / `controls.ini` はユーザーが PPSSPP 側で変更できる設定として扱い、
    既定の launcher 起動では自動補修しない。補修が必要な場合だけ
    `PLUMOS_A30_PSP_CONTROLS=standard` を明示する。
- `/mnt/SDCARD/plumos/config/input/mapping.json`
  - plumOS input mapping、hotkey、物理ボタン連動。

## System Settings

System Settings は `/mnt/SDCARD/plumos/config/system/settings.json` を読み書きします。
stockOS の `/config/system.json` とは切り離します。

現在のキー:

- `volume`: `0..20`
- `brightness`: `1..20`
- `lumination`: `0..10`
- `contrast`: `0..20`
- `hue`: `0..20`
- `saturation`: `0..20`
- `language`: `en.lang`, `ja.lang` など。対応する辞書は
  `/mnt/SDCARD/plumos/share/frontend/lang/` から読み込む
- `timezone`: POSIX TZ 文字列。既定値は `JST-9`

2026-06-20 時点では、`Language` は frontend の `.lang` 辞書を切り替え、
Settings の項目名と説明欄へ反映します。`Language` 以外の値は保存直後と FE 起動時に A30
runtime へ反映します。`volume` は `plumos-volume-control` で ALSA default PCM の
`Soft Volume Master` へ反映し、RetroArch/standalone emulator は ALSA `default` を通すことで
同じ音量設定を使います。`brightness` は
`/sys/devices/virtual/disp/disp/attr/lcdbl`、`lumination` / `contrast` / `hue` /
`saturation` は `/sys/devices/virtual/disp/disp/attr/enhance` を使います。
`Soft Volume Master` は ALSA default PCM を一度 open した後に作られるため、
FE と launcher は必要に応じて短い無音再生で初期化してから反映します。
Brightness は `1..20` を保存し、RAW は
`2,3,4,5,6,7,8,9,10,26,43,59,75,92,108,125,141,157,174,190` へ割り当てます。
`timezone` は plumOS config を原本とし、保存時・FE 起動時・MainUI wrapper 起動時に
`TZ` 環境と runtime `/etc/TZ` へ反映します。stockOS の `/config/system.json` には
書き込みません。手動時刻設定は選択中の timezone のローカル時刻として入力し、
UTC epoch に変換して OS 時刻へ適用します。

### Factory Reset

System Settings の `Factory Reset` は `plumos-factory-reset` を呼び出し、
`factory-defaults/{ra,pico,sa}/` に保存された emulator 設定だけを復元します。
実行項目は誤操作を避けるため A ボタン 2 回で確定します。

復元前の既存ファイルは
`/mnt/SDCARD/plumos/backups/factory-reset/<timestamp>/<target>/` に退避します。
復元は target 内の file 単位の上書きで行い、対象外の save data、BIOS、ROM、
thumbnail、log は削除しません。

## Performance Settings

Performance Settings は新しい cpufreq 仕様を先に作るのではなく、既存の
`plumos-text-ui core system|rom ... --cpu --freq --cores` と
`/mnt/SDCARD/plumos/state/frontend/core-overrides.json` に接続します。
`systems.json` の `default_cpu_policy` / `default_cpu_freq_khz` を既定値とし、
system override、ROM override の順で上書きします。plumOS default は全systemで
`648MHz` / `2 cores` に揃えます。

2026-06-09 時点では START `Performance Settings` から system を選び、
`CPU freq` と `CPU Cores` を左右で変更できます。`CPU freq` の候補は
`648/816/1200/1344 MHz` の固定値だけで、予測しづらい `keep` はユーザー向けから
削除します。保存は controller UI が
`plumos-text-ui core system <system> --cpu ... --freq ... --cores ...` を呼ぶことで
`core-overrides.json` へ入ります。`Reset to Default` は `--clear-cpu` を呼び、
`systems.json` の `648MHz` / `2 cores` default へ戻します。
