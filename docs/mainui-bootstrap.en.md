# MainUI Bootstrap

The stock A30 boot flow runs `/mnt/SDCARD/miyoo/app/MainUI` directly. plumOS
uses that path as a small wrapper and moves the actual frontend entry point to
`/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali`.

## Policy

- Back up stock MainUI to `/mnt/SDCARD/miyoo/app/MainUI.stock`.
- Replace `/mnt/SDCARD/miyoo/app/MainUI` with a shell wrapper.
- Launch `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` as the normal FE.
- Stop stock `keymon` after stock `/etc/main` starts the wrapper path.
- Run `/mnt/SDCARD/plumos/bin/plumos-stock-services boot` to stop StockOS
  `MtpDaemon`, `adbd`, and `sysntpd`, then normalize `/mnt/SDCARD` to a vfat
  UTF-8 mount when StockOS mounted it with ASCII filename conversion.
- After SD-card normalization, run `plumos-sdcard-cleanup` in the background to
  remove macOS/Windows sidecar files (`._*`, `.DS_Store`, `Thumbs.db`,
  `desktop.ini`, and `__MACOSX`). The normal scope is limited to `Roms`/`roms`
  and `Images`, and FE startup does not wait for cleanup completion. Full-SD
  cleanup stays available through `--all` for manual diagnostics.
- The controller UI also starts the same cleanup in the background when entering
  or returning to the START menu. Short repeated runs are throttled by the FE,
  and user input does not wait for cleanup completion.
- The ROM scanner always excludes sidecar files from ROM/thumbnail candidates
  and starts a no-cache-invalidation cleanup in the background at scan startup.
  Scan correctness comes from the ignore rules; physical deletion happens
  asynchronously.
- Do not run `plumos-network-rescue` automatically at boot.
- Run `plumos-network-control --wifi on`, but if `wlan0` already has an IP
  address, preserve the active wpa_supplicant/udhcpc runtime and only refresh
  runtime status / SSH. Only attempt saved-config DHCP/IP acquisition when no IP
  is present.
- Start SSH and enabled network services through `plumos-network-services start-enabled`.
- If `wlan0` already has an IP address, call `plumos-stock-services network-ready`
  to start plumOS-managed `ntpd`.
- Do not expose Network Recovery from the START menu or Network Settings.
- After the normal FE initializes the Mali renderer and completes its first
  frame, it creates `/tmp/plumos-fe-ready`. If this flag exists, the wrapper
  does not fall back to stock MainUI even if the FE exits later.
- Fall back to the legacy `plumos-frontend`, then stock MainUI, only if the
  controller UI is missing or exits before that first successful frame.
- Write logs under `/mnt/SDCARD/plumos/logs/`.
- Keep rollback possible through SD-card file changes only.

## Package Build

```sh
./scripts/build-bootstrap-package.sh
```

Outputs:

```text
dist/plumos-bootstrap/
dist/plumos-bootstrap.tar.gz
```

## Install

Run this while SSH is active on the A30.

```sh
A30_TARGET=root@192.168.10.165 ./scripts/install-mainui-wrapper-a30.sh
```

The install script:

- Deploys `dist/plumos-bootstrap` to `/mnt/SDCARD`.
- Creates `/mnt/SDCARD/miyoo/app/MainUI.stock` if it does not exist.
- Replaces `/mnt/SDCARD/miyoo/app/MainUI` with the wrapper.

The bootstrap package does not include the controller UI binary. Build and
deploy the frontend package separately with `./scripts/docker-build.sh frontend`.
At startup the wrapper does not run `plumos-network-rescue`; it shows the normal
FE, preserves an already-connected Wi-Fi runtime, only tries saved-config Wi-Fi
when disconnected, and starts enabled network services, including SSH, in the
background.
Left/Right are not used for confirm/run/back/cancel; A confirms/runs and B
backs/cancels.

Network recovery for cases where SSH is unavailable should be designed as a
separate shell script launched directly from StockOS MainUI, not as an FE route.

## FE Control

Use this single entry point for normal FE operations:

```sh
./scripts/a30-fe-control.sh status
./scripts/a30-fe-control.sh restart
./scripts/a30-fe-control.sh stop
./scripts/a30-fe-control.sh start
```

`plumos-fe-control stop` creates `/tmp/plumos-fe-paused`, then stops the FE and
wrapper. Stock `/etc/main` will run the wrapper again, but the wrapper waits
while the pause file exists instead of launching the FE. `start` and `restart`
remove the pause file and verify that `/dev/fb0` is owned by exactly one plumOS
FE process after startup.

## Boot Splash

The StockOS `LOADING` screen is not a kernel logo. `/etc/init.d/boot` extracts
`/usr/miyoo/res/loading.data.tgz` and writes it to `/dev/fb0` from userland.
plumOS does not modify the rootfs; instead, `MainUI.wrapper` redraws the screen
with `/mnt/SDCARD/plumos/bin/plumos-boot-splash` immediately after the wrapper
starts. The helper exits as soon as it has drawn the splash, so it does not block
the following SD-card UTF-8 remount. If
`/mnt/SDCARD/plumos/config/disable-mainui-wrapper` exists, the splash is skipped.

User-provided splash art lives at
`/mnt/SDCARD/plumos/config/frontend/boot-splash.png`. The recommended image is a
landscape `640x480` PNG, 24-bit/32-bit sRGB. The A30 framebuffer is physically
`480x640`, but plumOS treats the screen as the same `640x480` landscape logical
space used by the UI. If the file is missing or cannot be decoded as PNG, the
built-in splash is used. Images with other sizes are aspect-fit inside the
screen, with the built-in splash background filling any margins.

## Disable

To keep the wrapper installed but boot stock MainUI directly, create this file
on the A30:

```sh
touch /mnt/SDCARD/plumos/config/disable-mainui-wrapper
```

Remove it to re-enable the wrapper path:

```sh
rm -f /mnt/SDCARD/plumos/config/disable-mainui-wrapper
```

## Restore

Restore stock MainUI:

```sh
A30_TARGET=root@192.168.10.165 ./scripts/restore-mainui-wrapper-a30.sh
```

When editing the SD card directly, this also restores stock MainUI:

```sh
cp /mnt/SDCARD/miyoo/app/MainUI.stock /mnt/SDCARD/miyoo/app/MainUI
```

## Logs

```text
/mnt/SDCARD/plumos/logs/mainui-wrapper.log
/mnt/SDCARD/plumos/logs/plumos-controller-ui-mali.log
/mnt/SDCARD/plumos/logs/plumos-frontend.log
/mnt/SDCARD/plumos/logs/stock-services.log
/mnt/SDCARD/plumos/logs/sdcard-cleanup.log
```
