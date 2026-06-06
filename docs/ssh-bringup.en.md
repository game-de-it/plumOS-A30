# SSH Bring-Up

This project uses Dropbear for the first remote shell because it is small,
embedded-friendly, and can be built as a static armhf binary.

The current build uses Dropbear `2026.91`, published by the upstream project on
2026-05-10. The pinned tarball hash is:

```text
defa924475abf6bc1e74abc00173e46bfdc804bd47caafa14f5a4ef0cc76da34
```

This is a development access kit. The Dropbear build relaxes
`authorized_keys` ownership/mode checks so the key file can live on the A30 SD
card even when the stock rootfs is read-only and the SD card is FAT/exFAT.

## Build

```sh
./scripts/build-ssh-kit.sh
```

Optional: if an ARM strip tool is available, binaries are stripped
automatically. To force Docker-based stripping on this workstation:

```sh
PLUMOS_DOCKER_STRIP=1 ./scripts/build-ssh-kit.sh
```

## Prepare SD Card

1. Edit `dist/plumos-a30-ssh-kit/plumos/ssh/etc/authorized_keys`.
2. Put one SSH public key on a single line.
3. Copy all contents of `dist/plumos-a30-ssh-kit/` to the SD card root.

The copied SD card should contain:

```text
/mnt/SDCARD/plumos/ssh/start-ssh.sh
/mnt/SDCARD/plumos/ssh/bin/dropbear
/mnt/SDCARD/plumos/ssh/bin/dropbearkey
/mnt/SDCARD/plumos/ssh/bin/scp
/mnt/SDCARD/plumos/ssh/etc/authorized_keys
/mnt/SDCARD/Roms/PORTS/Start SSH.sh
/mnt/SDCARD/Roms/PORTS/Stop SSH.sh
```

## Start

On the Miyoo A30, open Ports and run `Start SSH`.

The launcher writes logs to:

```text
/mnt/SDCARD/plumos/ssh/log/dropbear.log
/mnt/SDCARD/plumos/ssh/log/network.txt
```

If the IP address is not visible in the UI/router, remove the SD card and check
`network.txt`; it dumps any available network commands and useful `/proc/net`
files.

If the log says no public key was found, replace
`plumos/ssh/etc/authorized_keys` on the SD card with a real workstation public
key. Commented example lines are ignored.

## Connect

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

If you use a non-default key:

```sh
ssh -i ~/.ssh/YOUR_KEY -p 2222 root@A30_IP_ADDRESS
```

## Collect Device Information

After SSH works:

```sh
./scripts/collect-a30-info.sh root@A30_IP_ADDRESS
```

The collector writes a timestamped directory under `artifacts/`.
