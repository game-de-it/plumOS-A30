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
- [x] Document that stock `config.json` is inventory/migration input, not the
  new plumOS specification.
- [x] Resolve `rompath`, `imgpath`, `gamelist`, and `launchlist` and check
  existence/counts.
- [x] Confirm ROM filename to artwork filename matching rules.
- [ ] Read the stock `gamelist` XML data schema.
- [ ] Recognize existing ROMs, artwork, saves, and states without moving them.
- [x] Implement `extlist` ROM count filtering in the compatibility scanner.
- [ ] Implement `extlist` filtering in the frontend ROM list.
- [x] Detect `launchlist` entries in the compatibility scanner.
- [ ] Support alternate launchers/cores through UI and launch profiles.
- [x] Detect existence and entry count for `Roms/recentlist.json` in the
  compatibility scanner.
- [ ] Parse/update a `Roms/recentlist.json` compatible recent list.
- [ ] Reproduce launch compatibility for `EMU_DIR`, `LD_LIBRARY_PATH`, ROM path
  `$1`.
- [ ] Eventually move away from shell launch scripts toward plumOS launch
  profiles.
- [x] Add a decision record template for any proposed direct reuse of stock
  behavior.

## Phase 6 - Frontend Implementation

- [x] Design a plumOS-native system/app/theme data model.
- [ ] Create the initial `systems.json` seed.
- [ ] Build a ROM scan prototype using Miyoo/ROCKNIX directory aliases.
- [ ] Build an on-enter per-system ROM directory re-scan prototype.
- [ ] Measure first text-mode display time with 1000 dummy ROM files.
- [ ] Implement a START menu UI model for settings/apps/reboot/shutdown.
- [ ] Implement system default core selection with SELECT on a highlighted TOP
  system.
- [ ] Implement per-ROM core override selection with SELECT on a highlighted ROM
  entry.
- [ ] Implement core priority as ROM override > system default > plumOS
  recommended > auto detect.
- [ ] Store core selection as an extensible launch profile id, not only a core
  id.
- [ ] Implement favorite toggle in the ROM list and open Favorites from the
  START menu.
- [ ] Add an optional setting to show Favorites as a virtual TOP system.
- [ ] Manage auto-resume target ROM/core/launch profile/pending state in
  `resume-session.json`.
- [ ] Auto-launch the same ROM/core/launch profile on boot when a pending resume
  session exists.
- [ ] Build a minimal controller-first frontend prototype.
- [ ] Implement system list, ROM list, recents, favorites, and settings.
- [ ] Decide how to load themes, fonts, and artwork.
- [ ] Implement thumbnail lookup that preserves paths relative to the ROM alias
  root.
- [ ] Implement thumbnail lookup with subdirectory priority, flat fallback, and
  placeholder fallback.
- [ ] Design a plumOS theme model that separates theme, layout preset, and
  frontend behavior.
- [ ] Define the schema for `/mnt/SDCARD/plumos/themes/<theme_id>/theme.json`.
- [ ] Limit themes to colors, fonts, backgrounds, system icons, selection style,
  spacing, thumbnail frames, and sound effects.
- [ ] Prevent themes from changing behavior such as button mappings, START menu,
  SELECT core menu, favorites, ROM scanning, and resume.
- [ ] Implement a text-mode fallback that remains usable with default font/color
  even when theme assets or fonts are missing/broken.
- [ ] Keep stock theme compatibility out of the initial spec; consider it later
  only as an importer if needed.
- [ ] Decide how to handle A30 settings such as brightness, volume, Wi-Fi, and
  keymap.
- [ ] Run minimal SDL/input/audio/video test binaries on the A30.
- [ ] Compare keeping stock `keymon` with reading `/dev/input/event*` directly.
- [ ] Investigate how to show a Sleep/Shutdown/Cancel menu on short power-key
  press while RetroArch is running.

## Phase 7 - RetroArch and Core Runtime

- [ ] Build/test RetroArch `v1.22.2` for the A30 armv7 hard-float environment.
- [ ] Place RetroArch at `/mnt/SDCARD/plumos/retroarch/bin/retroarch`.
- [ ] Place cores under `/mnt/SDCARD/plumos/retroarch/cores`.
- [ ] Stop depending on `HOME=/mnt/SDCARD/RetroArch`.
- [ ] Manage per-system/core differences through `--config` and override configs.
- [ ] Update libretro cores per system in stages.
- [ ] Validate boot, performance, save/state, input, and audio/video per core.
- [ ] Validate RetroArch Auto Save State / Auto Load State for the plumOS resume
  flow.
- [ ] Check whether Network Control Interface or an equivalent mechanism can run
  `SAVE_STATE`, `CLOSE_CONTENT`, and `QUIT` safely.
- [ ] Build a safe shutdown script that runs save state, save RAM flush,
  RetroArch exit, and `sync` before shutdown.
- [ ] Check whether true suspend/resume works on the A30; if not, evaluate a
  pseudo-sleep policy.

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
