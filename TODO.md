# TODO

## SSH Bring-Up

- Build the SD card SSH kit.
- Add a workstation public key to `plumos/ssh/etc/authorized_keys`.
- Boot the Miyoo A30 and launch `Start SSH` from Ports.
- Connect as `root` on port `2222`.
- Run `scripts/collect-a30-info.sh root@A30_IP_ADDRESS`.

## Next Investigation Targets

- Build a safe MainUI wrapper with rollback so SD-card frontend replacement can
  be tested without touching rootfs.
- Prototype a compatibility frontend that reads `Emu`, `RApp`, `App`,
  `Themes`, `Roms`, and `Imgs` using the stock JSON schema.
- Preserve launch-script behavior: `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path as
  `$1`, recent-list updates, CPU/performance helpers, and alternate launchers.
- Keep stock `keymon` initially and verify input events before replacing it.
- Bring the SDL/toolchain reference into a reproducible local build path.
- Build/test RetroArch v1.22.2 for A30's armv7 hard-float/glibc 2.23 runtime.
- Update libretro cores in validated batches instead of replacing all cores at
  once.
- Decide whether SSH should remain a dev-only package or become a CFW service.
- Define release packaging and GitHub artifact workflow.
