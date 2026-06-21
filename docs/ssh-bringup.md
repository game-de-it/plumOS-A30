# SSH Bring-Up

This project uses Dropbear for the first remote shell because it is small,
embedded-friendly, and can be built as a static armhf binary.

The current build uses Dropbear `2026.91`, published by the upstream project on
2026-05-10. The pinned tarball hash is:

```text
defa924475abf6bc1e74abc00173e46bfdc804bd47caafa14f5a4ef0cc76da34
```

This is the plumOS SSH access kit. The A30 stock rootfs is read-only, so the SSH
password is managed from the SD card at
`/mnt/SDCARD/plumos/ssh/etc/password.hash` instead of stock `/etc/shadow`.
Public key authentication is still supported as a secondary path. The Dropbear
build relaxes `authorized_keys` ownership/mode checks so the key file can live
on the A30 SD card even when the stock rootfs is read-only and the SD card is
FAT/exFAT.

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

1. Copy all contents of `dist/plumos-a30-ssh-kit/` to the SD card root.
2. Optionally put one SSH public key on a single line in
   `dist/plumos-a30-ssh-kit/plumos/ssh/etc/authorized_keys`.

The copied SD card should contain:

```text
/mnt/SDCARD/plumos/ssh/start-ssh.sh
/mnt/SDCARD/plumos/ssh/bin/dropbear
/mnt/SDCARD/plumos/ssh/bin/dropbearkey
/mnt/SDCARD/plumos/ssh/bin/scp
/mnt/SDCARD/plumos/ssh/etc/password.hash
/mnt/SDCARD/plumos/ssh/etc/authorized_keys.example
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

If `password.hash` is missing, `start-ssh.sh` recreates the hash for the default
password `plumos`. To use public key authentication, place a real workstation
public key in `plumos/ssh/etc/authorized_keys` on the SD card. Commented example
lines are ignored.

## Connect

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

Default password:

```text
plumos
```

If you use a non-default key:

```sh
ssh -i ~/.ssh/YOUR_KEY -p 2222 root@A30_IP_ADDRESS
```

The SSH shell and `ssh host command` PATH prefer the plumOS runtime.

```text
/mnt/SDCARD/plumos/gnu/bin:/mnt/SDCARD/plumos/bin:/usr/miyoo/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

The `ssh host command` PATH comes from the Dropbear build-time defaults. For an
interactive login, BusyBox ash reads the stockOS `/etc/profile`, which resets
PATH back to the stock value. Dropbear therefore passes
`ENV=/mnt/SDCARD/plumos/ssh/etc/ashrc`, and ash reads that SD-card file after
`/etc/profile` to restore the plumOS PATH. The rootfs `/etc/profile` is not
modified.

This makes `crc32`, `unzip`, `wget`, `plumos-fe-control`, and other plumOS
tools available without full paths immediately after login. Starting
`start-ssh.sh` with `PLUMOS_SSH_PATH` overrides only the launcher-side PATH.
The per-session default PATH is compiled into Dropbear and mirrored in
`plumos/ssh/etc/ashrc`, so release changes should update `DEFAULT_ROOT_PATH` in
`scripts/build-ssh-kit.sh`, update `package/ssh-kit/plumos/ssh/etc/ashrc`, and
rebuild the SSH kit.

Interactive SSH command history is configured by `plumos/ssh/etc/ashrc` with
`HISTSIZE=9999` and is stored in
`/mnt/SDCARD/plumos/state/ssh/ash_history`. Override the path with
`PLUMOS_SSH_HISTFILE` and the count with `PLUMOS_SSH_HISTSIZE`. The history file
stores entered commands verbatim, so avoid putting SSIDs, passwords, tokens, or
other sensitive values directly on command lines. The stock `/bin/sh` build does
not save history to a file, so interactive SSH sessions switch once from
`ashrc` into `/mnt/SDCARD/plumos/bin/busybox ash -i`. Non-interactive commands
such as `ssh host command` are not affected.

When BusyBox `ls` writes bare names directly to an interactive SSH TTY, it may
replace UTF-8 Japanese file names with `?`. The `ashrc` file defines an
interactive `ls` shell function that sends BusyBox `ls` stdout through a
temporary non-TTY path only for TTY output, preserving UTF-8 file names. This
does not affect non-interactive commands such as `ssh host command`.

## Collect Device Information

After SSH works:

```sh
./scripts/collect-a30-info.sh root@A30_IP_ADDRESS
```

The collector writes a timestamped directory under `artifacts/`.
