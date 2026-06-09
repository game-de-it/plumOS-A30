# plumOS USB Disk Mode

この文書は、FunKey-OS の USB Mass Storage 方式を参考にした plumOS の実験機能
`USB Disk Mode` の初期設計です。

## 目的

USB 接続時に A30 を PC から通常の USB drive として見せ、ROM や save file を高速かつ
安定して転送できるようにします。MTP daemon は使わず、kernel の USB mass storage
function に SD card partition を渡します。

## FunKey-OS から得た方針

FunKey-OS は MTP ではなく USB Mass Storage を使います。重要なのは、host PC へ
block device を渡す前に、FunKey 側で対象 partition を unmount している点です。

plumOS でも同じ原則を守ります。

- host PC と A30 が同じ FAT32 partition を同時に writable mount しない。
- USB Disk Mode に入る前に FE、emulator、file-transfer service、SSH を停止する。
- `/mnt/SDCARD` を unmount できた場合だけ USB mass storage を有効化する。
- PC 側で eject した後に USB Disk Mode を抜け、fsck、remount、FE/SSH restart を行う。

## A30 の実機状態

A30 stock kernel には Android USB gadget 互換の mass storage entry が存在します。

```text
/sys/class/android_usb/android0/functions
/sys/class/android_usb/android0/f_mass_storage/lun/file
/sys/class/android_usb/android0/f_mass_storage/lun/ro
/sys/class/android_usb/android0/f_mass_storage/lun/nofua
```

共有対象の初期候補は次の partition です。

```text
/dev/mmcblk0p1 -> /mnt/SDCARD
```

FunKey-OS の configfs/libcomposite 方式とは違い、A30 では
`/sys/class/android_usb/android0` を通して `mass_storage` function を選択します。

## Helper

hidden helper として次を追加します。

```sh
/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode
```

安全のため、通常実行では `enter` は拒否されます。実験時だけ次の明示フラグを要求します。

```sh
PLUMOS_USB_DISK_MODE_EXPERIMENTAL=1 \
PLUMOS_USB_DISK_MODE_CONFIRM=UNMOUNT_SDCARD \
/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode enter
```

SSH からの `enter` は既定で拒否します。USB Disk Mode は `/mnt/SDCARD` を unmount し、
SSH daemon も停止するためです。

## 想定 UI flow

検証用の入口は `Network Settings -> USB Disk Mode` に置きます。誤操作を避けるため、
項目選択後は確認画面を挟み、確認画面の A で開始、B で Network Settings へ戻ります。

1. 確認画面を出す。
2. 「このモード中はA30からSDカードを使えません。PCで安全に取り外してからUSBを抜く」
   ことを表示する。
3. FE を停止し、helper を tmpfs へ移して実行する。
4. PC に USB drive として表示する。
5. PC 側で eject 後、USB cable disconnect または A30 側の復帰操作で終了する。
6. fsck、remount、network recovery、FE restart を行う。

## 未解決項目

- A30 側で USB cable disconnect 以外の復帰入力をどう受けるか。
- USB Disk Mode 中に画面へ案内を出し続ける rootfs/tmpfs resident UI が必要か。
- `nofua=0` と `nofua=1` の速度/安定性比較。
- macOS/Windows/Linux で eject 後の dirty bit と再 mount を検証する。
- 実 transfer benchmark を FTP/SFTP/Samba と同じ条件で測る。
