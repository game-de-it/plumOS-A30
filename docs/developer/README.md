# plumOS Developer Guide

This guide is the entry point for developers who build, modify, test, or
release plumOS.

## Start Here

1. [Docker Build Guide](build.md)
2. [Feature Reverse Lookup](feature-index.md)
3. [plumOS Design Policy](../plumos-design-policy.md)
4. [Config Layout Policy](../plumos-config-layout.md)
5. [Runtime Package](../runtime-package.md)
6. [SD Root Package](../sdroot-package.md)
7. [Release Artifacts](../release-artifacts.md)

## Technical Layout

plumOS normally avoids writing to the A30 rootfs/NAND. The system is built as an
SD-card runtime.

```text
/mnt/SDCARD/
  miyoo/                    StockOS SD payload + plumOS MainUI wrapper
  plumos/
    bin/                    frontend/launcher/helper
    config/                 plumOS-owned persistent config
    lib/                    bundled runtime libraries
    retroarch/              RetroArch runtime and libretro cores
    emulators/              standalone emulator binaries
    apps/                   Apps menu applications
    python/                 Python/Pyxel runtime
    ssh/                    Dropbear SSH/SFTP runtime
    state/                  runtime state
    logs/                   logs
```

## Main Components

| Component | Role |
| --- | --- |
| frontend | TOP/ROM list/START/Settings/Power/Apps UI |
| bootstrap | MainUI wrapper that starts plumOS from the StockOS boot flow |
| userland | BusyBox, rsync, ip, ss, strace, and helpers |
| network-services | SSH/FTP/SFTP/Samba service management |
| joystickd | Converts A30 input to emulator-friendly gamepad events |
| retroarch-practical | Main RetroArch runtime |
| libretro-cores | libretro cores used by RetroArch and PicoArch |
| picoarch | Lightweight libretro frontend |
| standalone-emulators | PPSSPP, ScummVM, EasyRPG, OpenBOR, PCSX-ReARMed, and related launchers |
| python-runtime | Python 3 / pip / Pyxel runtime |
| pyxel-a30 | Pyxel launcher and A30 execution path |
| nextcommander | File manager |
| music-player | plumOS native music player |

## Important Rules

- Keep plumOS persistent config under `/mnt/SDCARD/plumos/config/`.
- Do not use stockOS-owned `/config/system.json` as normal plumOS config.
- Use `scripts/a30-fe-control.sh` for FE start/stop/restart/status.
- Use `scripts/run-a30.sh` for A30 commands.
- Use `scripts/deploy-a30.sh` for deployment.
- Keep exactly one `/dev/fb0` owner during UI/emulator verification.
- Do not commit or log ROMs, BIOS files, network secrets, or personal data.

## Detailed References

- [A30 System Notes](../a30-system-notes.md)
- [Feature Reverse Lookup](feature-index.md)
- [A30 Input Policy](../a30-input-policy.md)
- [A30 joystickd](../a30-joystickd.md)
- [A30 Stock SDL Video](../a30-stock-sdl-video.md)
- [MainUI Bootstrap](../mainui-bootstrap.md)
- [Frontend Prototype](../frontend-prototype.md)
- [Frontend Data Model](../frontend-data-model.md)
- [A30 UI Design](../a30-ui-design.md)
- [Network Services](../network-services.md)
- [USB Disk Mode](../usb-disk-mode.md)
- [Docker Toolchain Details](../docker-toolchain.md)
- [Emulator Runtime Manifest](../emulator-runtime-manifest.md)
- [Emulator Issue Triage](../emulator-issue-triage.md)
- [libretro Core Version Inventory](../libretro-core-version-inventory.md)

## Japanese Counterpart

- [Japanese developer guide](README.ja.md)
