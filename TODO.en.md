# TODO

## Done

- Build a development SSH kit that starts from the SD card.
- Allow workstation public keys under `plumos/ssh/etc/authorized_keys`.
- Start SSH from Ports on the A30.
- Collect device information with `scripts/collect-a30-info.sh root@A30_IP_ADDRESS`.
- Document boot flow, mounts, Wi-Fi, stock frontend, and RetroArch layout.

## Next Implementation And Investigation

- Build a rollback-safe `MainUI` wrapper.
- Launch `/mnt/SDCARD/plumos/bin/plumos-frontend` from that wrapper.
- Package the `/mnt/SDCARD/plumos` runtime directory layout.
- Add `plumos-env` without depending on stock `/mnt/SDCARD/miyoo/lib`.
- Manually run a plumOS frontend prototype while keeping stock MainUI.
- Build a compatibility layer that reads the stock JSON schema from `Emu`,
  `RApp`, `App`, `Themes`, `Roms`, and `Imgs`.
- Recognize existing ROMs, artwork, saves, and states without moving them.
- Reproduce launch compatibility for `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path
  `$1`, `launchlist`, and `recentlist.json`.
- Run minimal SDL/input/audio/video test binaries on the A30.
- Compare keeping stock `keymon` with reading `/dev/input/event*` directly.
- Prepare a reproducible A30 sysroot/toolchain.
- Build/test RetroArch `v1.22.2` for the A30 armv7 hard-float environment.
- Update libretro cores per system, validating boot, performance, save/state,
  and input.
- Measure CPU governor, CPU online/offline, and clock policy to decide whether
  stock script behavior is actually valid.
- Check whether the Wi-Fi power sequence can be safely reproduced from plumOS.
- Decide whether SSH stays a development package or becomes a plumOS service.
- Define the GitHub release package/artifact workflow.
