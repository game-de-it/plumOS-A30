# plumOS Network Services

This document defines the initial file-transfer network services provided by plumOS.

## Policy

- Every service uses `/mnt/SDCARD/` as the shared home/root.
- plumOS executables, configuration, logs, and persistent state live under `/mnt/SDCARD/plumos/`.
- Do not modify the stock rootfs or `/config/system.json`.
- Network Settings shows `SSH`, `FTP`, `SFTP`, and `Samba` as checkboxes under the
  `NW Service` subpage. A toggles each service.
- ON means start + enable, and OFF means stop + disable. The state persists across reboot.
- To transfer many small ROM files efficiently, each service should expose a concurrent-transfer
  entry limit of 20. The normal recommendation is 10-way parallel transfer; 20 is headroom.

## Services

### SSH

- Implementation: Dropbear SSH
- Port: `2222`
- Authentication: default password `plumos`; the hash lives at
  `/mnt/SDCARD/plumos/ssh/etc/password.hash`. Optional key authentication uses
  `/mnt/SDCARD/plumos/ssh/etc/authorized_keys`

SSH is the plumOS remote shell entry point. If `ssh_enabled` is not set, it defaults to enabled
for compatibility with the existing development path. Turning SSH OFF in NW Service writes
`ssh_enabled=0`, and the boot wrapper or USB Disk Mode recovery will not start it automatically.
Because SFTP depends on SSH, turning SSH OFF also turns SFTP OFF. Turning SFTP ON enables SSH again.

### FTP

- Implementation: BusyBox `tcpsvd` + `ftpd`
- Port: `21`
- Shared root: `/mnt/SDCARD/`
- Authentication: anonymous/write-enabled in the first implementation
- Concurrent connection limit: `20`

FTP is the lightweight candidate if multi-file transfer tests are stable.

The FTP server advertises `UTF8` in `FEAT` and accepts `OPTS UTF8 ON`. If a Windows
client such as FileZilla sends Japanese ROM filenames as Shift-JIS/CP932, the A30 vfat
`utf8` mount cannot create those names and the upload fails with `553 Error`; some
clients present this as 533 or as a "cannot create file" error. For Japanese ROM filenames,
set FileZilla Site Manager's Charset to `Force UTF-8`. This is especially important when
transferring files directly from a NAS, where FileZilla's automatic charset mode may not
select UTF-8.

The SD card is mounted as vfat, so ROM filenames must avoid characters that FAT cannot store.
In particular, names containing `:`, `*`, `?`, `"`, `<`, `>`, `|`, or `\` cannot be saved even
as UTF-8, even with forced UTF-8 enabled. Rename files before transfer, for example by replacing `:` with ` - `. Case variants
such as `Roms/gbc` and `Roms/GBC` point at the same directory on the A30 vfat mount, so they are
not the main cause of this upload failure.

### SFTP

- Implementation: Dropbear SSH + OpenSSH `sftp-server`
- Port: same as SSH, `2222`
- Shared root: the SFTP server starts from `/mnt/SDCARD/`
- Authentication: existing SSH authentication
- Concurrent connection entry limit: Dropbear `MAX_UNAUTH_PER_IP=20`

SFTP is attached to the SSH daemon, so turning SFTP OFF must not stop SSH itself. OFF disables the
`/mnt/SDCARD/plumos/ssh/libexec/sftp-server` entry point while keeping SSH shell access alive.

### Samba

- Implementation: Samba `smbd`
- Port: `445`
- Share name: `SDCARD`
- Shared root: `/mnt/SDCARD/`
- URL example: `smb://A30_IP/SDCARD`
- Authentication: `plumos` / `plumos`
- Concurrent connection limit: `20`

The Samba setup prioritizes mounting from Windows and macOS as a network drive. The first
implementation maps `plumos` to `root` inside Samba and forces the share user to `root`. On A30,
NetBIOS discovery is not used; mount directly with `\\A30_IP\SDCARD` or `smb://A30_IP/SDCARD`.

## Persistent State

Service state is stored in:

```text
/mnt/SDCARD/plumos/config/network/services.conf
```

Keys:

```text
ssh_enabled=0|1
ftp_enabled=0|1
sftp_enabled=0|1
samba_enabled=0|1
```

The boot wrapper and network recovery call `plumos-network-services start-enabled` to restore only
enabled services.

## Commands

The service helper is:

```sh
/mnt/SDCARD/plumos/bin/plumos-network-services status ftp
/mnt/SDCARD/plumos/bin/plumos-network-services status ssh
/mnt/SDCARD/plumos/bin/plumos-network-services start ftp
/mnt/SDCARD/plumos/bin/plumos-network-services stop ftp
/mnt/SDCARD/plumos/bin/plumos-network-services start-enabled
```

`SERVICE` accepts `ssh`, `ftp`, `sftp`, `samba`, or `all`.

## Logs

```text
/mnt/SDCARD/plumos/logs/network-services.log
/mnt/SDCARD/plumos/logs/samba-*.log
```

Do not commit or log SSIDs, PSKs, or private filenames.

## Benchmark Results

As of 2026-06-10, macOS -> A30 upload results with 50 x 1MiB files and 10-way
parallel transfer are:

| Method | Result | Time | Throughput | Notes |
| --- | ---: | ---: | ---: | --- |
| USB Disk Mode | 50/50 success | 5.827 seconds | 8.58 MiB/s | macOS mounted `/Volumes/PLUMOS-A30`; timing ended after `sync`. SHA-256 matched. |
| FTP | 50/50 success | 8.645 seconds | 5.78 MiB/s | Network service candidate when speed matters. |
| SFTP | 50/50 success | 16.936 seconds | 2.95 MiB/s | Dropbear must use `MAX_UNAUTH_PER_IP=20`. |
| Samba | 50/50 success | 29.658 seconds | 1.69 MiB/s | On macOS, use `cp -X` to avoid xattr errors. |

SFTP also passed a 20-way smoke test with 20 x 1MiB files. The normal recommendation
for many small ROM files is 10-way parallel transfer, with the service entry limit
set to 20.

Current recommendation: use USB Disk Mode when a USB cable is available, FTP
when network speed is the priority, SFTP when secure standard-tool compatibility
matters, and Samba when Windows/macOS network-drive mounting is the priority.
Long-running checks for FE input and emulator-launch impact remain future
validation work.

USB Disk Mode is not a network service. See [USB Disk Mode](usb-disk-mode.en.md)
for details.
