# plumOS Network Services

This document defines the initial file-transfer network services provided by plumOS.

## Policy

- Every service uses `/mnt/SDCARD/` as the shared home/root.
- plumOS executables, configuration, logs, and persistent state live under `/mnt/SDCARD/plumos/`.
- Do not modify the stock rootfs or `/config/system.json`.
- Network Settings shows `FTP`, `SFTP`, and `Samba` as checkboxes. A toggles each service.
- ON means start + enable, and OFF means stop + disable. The state persists across reboot.

## Services

### FTP

- Implementation: BusyBox `tcpsvd` + `ftpd`
- Port: `21`
- Shared root: `/mnt/SDCARD/`
- Authentication: anonymous/write-enabled in the first implementation

FTP is the lightweight candidate if multi-file transfer tests are stable.

### SFTP

- Implementation: Dropbear SSH + OpenSSH `sftp-server`
- Port: same as SSH, `2222`
- Shared root: the SFTP server starts from `/mnt/SDCARD/`
- Authentication: existing SSH authentication

SFTP is attached to the SSH daemon, so turning SFTP OFF must not stop SSH itself. OFF disables the
`/mnt/SDCARD/plumos/ssh/libexec/sftp-server` entry point while keeping SSH shell access alive.

### Samba

- Implementation: Samba `smbd`
- Port: `445`
- Share name: `SDCARD`
- Shared root: `/mnt/SDCARD/`
- URL example: `smb://A30_IP/SDCARD`
- Authentication: `plumos` / `plumos`

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
/mnt/SDCARD/plumos/bin/plumos-network-services start ftp
/mnt/SDCARD/plumos/bin/plumos-network-services stop ftp
/mnt/SDCARD/plumos/bin/plumos-network-services start-enabled
```

`SERVICE` accepts `ftp`, `sftp`, `samba`, or `all`.

## Logs

```text
/mnt/SDCARD/plumos/logs/network-services.log
/mnt/SDCARD/plumos/logs/samba-*.log
```

Do not commit or log SSIDs, PSKs, or private filenames.

## Benchmark Plan

README recommendations should be updated after real measurements. Measure:

- Large single-file upload/download
- Many-small-file upload/download
- Stability during simultaneous multi-file transfer
- Usability from macOS Finder, Windows Explorer, FTP clients, and SFTP clients
- Whether transfer load interferes with FE input or emulator launch

Initial comparison targets are Samba for ease of use, FTP for lightweight transfer, and SFTP for
standard secure-tool compatibility.
