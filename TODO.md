# TODO

## SSH Bring-Up

- Build the SD card SSH kit.
- Add a workstation public key to `plumos/ssh/etc/authorized_keys`.
- Boot the Miyoo A30 and launch `Start SSH` from Ports.
- Connect as `root` on port `2222`.
- Run `scripts/collect-a30-info.sh root@A30_IP_ADDRESS`.

## Next Investigation Targets

- Confirm CPU, kernel, libc, init system, writable paths, and SD mount layout.
- Map stock/spruceOS boot flow and identify reliable autostart hooks.
- Inventory existing network tools and Wi-Fi configuration flow.
- Decide whether SSH should remain a dev-only package or become a CFW service.
- Bring the SDL/toolchain reference into a reproducible local build path.
- Define release packaging and GitHub artifact workflow.

