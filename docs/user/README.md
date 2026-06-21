# plumOS User Guide

This guide is for people using plumOS on a Miyoo A30. It focuses on installing
the SD-card package, placing ROMs and BIOS files, launching games, and changing
settings.

## Start Here

1. [Installation](install.md)
2. [Basic Operation](operation.md)
3. [SD Card Layout](storage.md)
4. [Supported Systems and Emulator Profiles](emulators.md)

## Core Ideas

- plumOS normally does not modify the A30 rootfs/NAND.
- The release archive is extracted to the root of the SD card.
- ROMs, BIOS files, save/state data, screenshots, and videos are not included.
- plumOS-owned configuration, logs, and runtime files live mainly under
  `plumos/`.
- StockOS SD-card runtime files are included for fallback and compatibility.

## Main Features

- plumOS frontend
- Text and Graphic UI modes
- RetroArch
- PicoArch
- Standalone emulators
- Pyxel runtime
- File manager
- Music player
- ROM thumbnail scraping
- FTP, SFTP, and Samba services
- USB Disk Mode
- SSH
- Emulator configuration factory reset

## Important Note

plumOS does not provide ROMs or BIOS files. Prepare any required game data and
BIOS files yourself from legal sources.

plumOS-authored files are MIT-licensed. Bundled third-party components and
StockOS-origin files keep their own licenses. See
[Third-party notices](../../THIRD_PARTY_NOTICES.md).

## Japanese Counterpart

- [Japanese user guide](README.ja.md)
