# MainUI Bootstrap

The stock A30 boot flow runs `/mnt/SDCARD/miyoo/app/MainUI` directly. plumOS
uses that path as a small wrapper and moves the actual frontend entry point to
`/mnt/SDCARD/plumos/bin/plumos-frontend`.

## Policy

- Back up stock MainUI to `/mnt/SDCARD/miyoo/app/MainUI.stock`.
- Replace `/mnt/SDCARD/miyoo/app/MainUI` with a shell wrapper.
- Launch `/mnt/SDCARD/plumos/bin/plumos-frontend` from the wrapper.
- Fall back to stock MainUI if the plumOS frontend is missing or exits with an
  error.
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

The current `plumos-frontend` is only a placeholder. It logs the boot path and
then intentionally exits non-zero so the wrapper falls back to stock MainUI.

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
/mnt/SDCARD/plumos/logs/plumos-frontend.log
```
