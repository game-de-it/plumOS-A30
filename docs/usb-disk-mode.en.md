# plumOS USB Disk Mode

This document defines the first experimental design for `USB Disk Mode`, based on
the USB Mass Storage approach used by FunKey-OS.

## Goal

When connected over USB, expose the A30 to a host PC as a normal USB drive for
fast and stable ROM/save-file transfer. This avoids MTP daemons and gives the SD
card partition directly to the kernel USB mass-storage function.

## Policy From FunKey-OS

FunKey-OS uses USB Mass Storage, not MTP. Its key safety rule is that the device
unmounts the shared partition locally before giving the block device to the host
PC.

plumOS follows the same rule.

- Do not let the host PC and A30 writable-mount the same FAT32 partition at the
  same time.
- Before entering USB Disk Mode, stop the FE, emulators, file-transfer services,
  and SSH.
- Stop StockOS `MtpDaemon` / `adbd` while USB Disk Mode is active because they
  also touch the USB gadget, then restart them on return if they were running.
- The USB cable may already be connected before entering USB Disk Mode. On
  entry, the helper disables the gadget, switches to mass_storage, and re-enables
  it so the host PC re-enumerates the device.
- Enable USB mass storage only after `/mnt/SDCARD` is unmounted.
- After the host PC ejects the drive, leave USB Disk Mode, run fsck, remount the
  SD card, and restart FE/SSH.
- Because the FE is launched from `/mnt/SDCARD/miyoo/app`, the helper copies
  itself to tmpfs and changes its working directory to `/` before unmounting the
  SD card.
- If entry fails partway through, the helper attempts to restore the USB gadget,
  SD mount, SSH, and FE.

## A30 Device State

The A30 stock kernel exposes Android USB gadget mass-storage entries:

```text
/sys/class/android_usb/android0/functions
/sys/class/android_usb/android0/f_mass_storage/lun/file
/sys/class/android_usb/android0/f_mass_storage/lun/ro
/sys/class/android_usb/android0/f_mass_storage/lun/nofua
```

The initial backing partition candidate is:

```text
/dev/mmcblk0p1 -> /mnt/SDCARD
```

Unlike FunKey-OS's configfs/libcomposite setup, the A30 uses the Android USB
gadget sysfs interface under `/sys/class/android_usb/android0`.

## Helper

The hidden helper is:

```sh
/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode
```

For safety, `enter` refuses to run by default. Experiments must explicitly set:

```sh
PLUMOS_USB_DISK_MODE_EXPERIMENTAL=1 \
PLUMOS_USB_DISK_MODE_CONFIRM=UNMOUNT_SDCARD \
/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode enter
```

By default, `enter` also refuses to run from SSH because USB Disk Mode unmounts
`/mnt/SDCARD` and stops the SSH daemon.

## Validation Logs

Normal plumOS files live under `/mnt/SDCARD/plumos/`. During USB Disk Mode,
however, `/mnt/SDCARD` is intentionally unmounted, so a failure followed by an OS
reboot cannot leave diagnostics on the SD card.

For validation only, the helper also writes a diagnostic log to the stockOS
`/config` partition:

```text
/config/plumos-usb-disk-mode.log
```

When the helper returns normally, it also copies the log to
`/mnt/SDCARD/plumos/logs/usb-disk-mode-last.log`. Before making the feature
normal, decide whether to keep this `/config` diagnostic exception or replace it
with an on-device recovery screen.

## Expected UI Flow

The validation entry lives at `Network Settings -> USB Disk Mode`. To avoid
accidental entry, selecting the item opens a confirmation screen: A starts the
mode, and B returns to Network Settings.

1. Show a confirmation screen.
2. Explain that the SD card is unavailable to A30 during this mode, and the user
   must eject the drive on the PC before unplugging USB.
3. Stop the FE and run the helper from tmpfs.
4. Present the SD card as a USB drive to the PC.
5. After host eject, exit on USB cable disconnect or an A30-side return action.
6. Run fsck, remount, run network recovery, and restart the FE.

## Transfer Benchmark

Measured on 2026-06-10 with macOS mounting the drive as `/Volumes/PLUMOS-A30`.
The test used the same conditions as FTP/SFTP/Samba: 50 x 1MiB files transferred
with 10-way parallelism, ending the timer after `sync`.

| Result | Time | Throughput | Verification |
| ---: | ---: | ---: | --- |
| 50/50 success | 5.827 seconds | 8.58 MiB/s | file sizes OK, SHA-256 matched |

## Completion Decision

As of 2026-06-10, the basic file-transfer flow is considered complete: Windows
and macOS recognize the device as USB Mass Storage, the macOS transfer benchmark
passed under the same conditions used for FTP/SFTP/Samba, the start screen gives
clear feedback, and the SD-card remount/FE return flow has been validated.

## Future Improvements

- Decide how to return from USB Disk Mode without relying only on USB disconnect.
- Decide whether a rootfs/tmpfs-resident screen is needed while the SD card is
  unmounted.
- Compare `nofua=0` and `nofua=1` for speed and reliability.
- Validate dirty-bit/remount behavior on macOS, Windows, and Linux.
