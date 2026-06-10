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
- Do not run `plumos-network-rescue` automatically at boot.
- Run `plumos-network-control --wifi on` from the saved Wi-Fi config to acquire
  DHCP/IP and start SSH.
- Start the SSH helper and enabled network services from the wrapper.
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
FE and starts saved-config Wi-Fi, the SSH helper, and enabled network services in
the background.
Left/Right are not used for confirm/run/back/cancel; A confirms/runs and B
backs/cancels.

Network recovery for cases where SSH is unavailable should be designed as a
separate shell script launched directly from StockOS MainUI, not as an FE route.

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
```
