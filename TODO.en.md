# TODO

## Phase 0 - Repository and Remote Access

- [x] Prepare the local git repository at `/Users/kroot/plumOS-A30`.
- [x] Build a development SSH kit that starts from the SD card.
- [x] Allow workstation public keys under `plumos/ssh/etc/authorized_keys`.
- [x] Start SSH from Ports on the A30.
- [x] Collect device information with `scripts/collect-a30-info.sh root@A30_IP_ADDRESS`.
- [x] Document boot flow, mounts, Wi-Fi, stock frontend, and RetroArch layout.
- [x] Use Japanese as the primary documentation language and `.en.md` for English.
- [ ] Finalize GitHub remote, license, and release policy before publication.

## Phase 1 - Docker Toolchain and Build Environment

- [x] Design a plumOS-specific Docker build environment.
- [x] Put the toolchain Dockerfile and helper scripts under `docker/`.
- [ ] Make the A30 sysroot reproducible through Docker builds.
- [ ] Build the frontend, helpers, RetroArch, and libretro cores inside Docker.
- [x] Build the frontend compatibility scanner inside Docker.
- [x] Collect Docker build output under `dist/` or a staging directory.
- [x] Keep build cache and large generated files out of git.
- [x] Document how to build and use the Docker image in Japanese and English.

## Phase 2 - plumOS Runtime Layout

- [ ] Package the `/mnt/SDCARD/plumos` runtime directory layout.
- [ ] Define the roles of `bin/`, `lib/`, `runtime/`, `frontend/`, `retroarch/`,
  `config/`, `state/`, `cache/`, `logs/`, and `ssh/`.
- [ ] Add `plumos-env` without depending on stock `/mnt/SDCARD/miyoo/lib`.
- [x] Build/deploy a static BusyBox for plumOS to avoid stock BusyBox quirks.
- [x] Install BusyBox applets as wrapper scripts instead of symlinks for vfat.
- [ ] Add `procps-ng`, `coreutils`, `util-linux`, and similar tools under
  `plumos/gnu/bin` for a more Debian-like workflow.
- [ ] Validate compatibility for `ps`, `top`, `df`, `free`, `tar`, `find`,
  `grep`, `sed`, `awk`, `ip`, `ss`, `lsof`, and `strace`.
- [ ] Validate the bundled dynamic linker/shared library strategy when dynamic
  linking is required.
- [ ] Run a smoke test on the device using only plumOS runtime paths.

## Phase 3 - Device Deployment Loop

- [x] Add a deploy script that transfers Docker-built artifacts to the A30.
- [x] Choose the SSH/SCP/rsync-equivalent transfer method.
- [x] Add helpers to run commands on the device and collect logs afterward.
- [ ] Make build -> deploy -> run -> collect logs work through one command.
- [ ] Document a rollback path that only requires SD-card changes.

## Phase 4 - Boot Bootstrap

- [x] Build a rollback-safe `MainUI` wrapper.
- [x] Launch `/mnt/SDCARD/plumos/bin/plumos-frontend` from the wrapper.
- [x] Fall back to stock MainUI if wrapper startup fails.
- [x] Write wrapper and frontend logs to `/mnt/SDCARD/plumos/logs`.
- [x] Manually run a plumOS frontend prototype while keeping stock MainUI.
- [ ] Confirm the A30 remains recoverable after reboot.

## Phase 5 - Frontend Compatibility Layer

- [x] Read stock `config.json` files from `Emu`, `RApp`, `App`, and `Themes`.
- [ ] Read stock data schema from `Roms`, `Imgs`, `gamelist`, and artwork.
- [ ] Recognize existing ROMs, artwork, saves, and states without moving them.
- [ ] Implement ROM filtering through `extlist`.
- [ ] Support alternate launchers/cores through `launchlist`.
- [ ] Handle a `Roms/recentlist.json` compatible recent list.
- [ ] Reproduce launch compatibility for `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path
  `$1`.
- [ ] Eventually move away from shell launch scripts toward plumOS launch
  profiles.

## Phase 6 - Frontend Implementation

- [ ] Build a minimal controller-first frontend prototype.
- [ ] Implement system list, ROM list, recents, favorites, and settings.
- [ ] Decide how to load themes, fonts, and artwork.
- [ ] Decide how to handle A30 settings such as brightness, volume, Wi-Fi, and
  keymap.
- [ ] Run minimal SDL/input/audio/video test binaries on the A30.
- [ ] Compare keeping stock `keymon` with reading `/dev/input/event*` directly.

## Phase 7 - RetroArch and Core Runtime

- [ ] Build/test RetroArch `v1.22.2` for the A30 armv7 hard-float environment.
- [ ] Place RetroArch at `/mnt/SDCARD/plumos/retroarch/bin/retroarch`.
- [ ] Place cores under `/mnt/SDCARD/plumos/retroarch/cores`.
- [ ] Stop depending on `HOME=/mnt/SDCARD/RetroArch`.
- [ ] Manage per-system/core differences through `--config` and override configs.
- [ ] Update libretro cores per system in stages.
- [ ] Validate boot, performance, save/state, input, and audio/video per core.

## Phase 8 - A30 System Policy Validation

- [ ] Measure CPU governor, CPU online/offline, and clock policy.
- [ ] Decide whether the stock CPU2/CPU3 offline behavior is valid.
- [ ] Compare `performance`, `ondemand`, and `interactive` governors.
- [ ] Restore CPU state reliably after game exit.
- [ ] Check whether the Wi-Fi power sequence can be safely reproduced from plumOS.
- [ ] Decide whether to keep stock Wi-Fi userland or bundle it with plumOS.
- [ ] Decide whether SSH stays a development package or becomes a plumOS service.

## Phase 9 - Packaging and Release

- [ ] Treat development Docker files as part of the project deliverables.
- [ ] Build the runtime package that is extracted to the SD card.
- [ ] Build a developer Docker/toolchain package.
- [ ] Decide how to split end-user releases and developer releases.
- [ ] Prepare license notices and upstream attribution.
- [ ] Add a GitHub Release package/artifact workflow.
- [ ] Verify install and rollback on a fresh-SD-card equivalent setup.
