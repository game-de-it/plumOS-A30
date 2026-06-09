# MainUI Bootstrap

The stock A30 boot flow runs `/mnt/SDCARD/miyoo/app/MainUI` directly. plumOS
uses that path as a small wrapper and moves the actual frontend entry point to
`/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali`.

## Policy

- Back up stock MainUI to `/mnt/SDCARD/miyoo/app/MainUI.stock`.
- Replace `/mnt/SDCARD/miyoo/app/MainUI` with a shell wrapper.
- Launch `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` as the normal FE.
- Run `/mnt/SDCARD/plumos/bin/plumos-network-rescue` automatically for boot
  recovery.
- Stop stock `keymon` after stock `/etc/main` starts the wrapper path.
- Open `Network Recovery` from the START menu; pressing A there reruns Wi-Fi
  init, DHCP, and dropbear SSH start.
- Fall back to the legacy `plumos-frontend`, then stock MainUI, if the
  controller UI is missing or exits with an error.
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
At startup the wrapper runs `plumos-network-rescue` in the background, then
shows the normal FE. The `Network Recovery` screen is reachable from the normal
FE START menu and pressing A there reruns
`/mnt/SDCARD/plumos/bin/plumos-network-rescue`. Left/Right are not used for
confirm/run/back/cancel; A confirms/runs and B backs/cancels.

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
```
