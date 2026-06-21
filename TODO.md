# TODO

This file tracks current work items only. Put work history, probe logs, and
rationale in `docs/`, commit history, or `artifacts/`. Do not append new history
to `HANDOFF.md`.

Japanese counterpart: [TODO.ja.md](TODO.ja.md)

## Phase 0 - Repository and Remote Access

- [x] Set up the local git repository at `/Users/kroot/plumOS-A30`.
- [x] Prepare the SD-card boot development SSH kit.
- [x] Enable SSH-based information gathering on the A30.
- [x] Document the boot flow, mounts, Wi-Fi, stock frontend, and RetroArch layout.
- [x] Use English `.md` documents as the default documentation and Japanese
      `.ja.md` files as counterparts.
- [ ] Decide the GitHub remote and release policy before public publishing.

## Phase 1 - Docker Toolchain and Build Environment

- [x] Prepare the plumOS Docker build environment.
- [x] Build the frontend, helpers, RetroArch, libretro cores, and standalone emulators inside Docker.
- [x] Collect Docker build outputs under `dist/` or a staging directory.
- [x] Keep build caches and large generated outputs out of git.
- [x] Record upstream versions and build options for the SDL2 runtime and minimal RetroArch.
- [ ] Make the A30 sysroot reproducible from the Docker build.
- [x] Record selected versions, tags, commits, and build options for emulator/core/helper libraries.

## Phase 2 - plumOS Runtime Layout

- [x] Package the runtime directory layout under `/mnt/SDCARD/plumos`.
- [x] Split responsibilities across `config/frontend`, `config/system`, `config/network`, and `config/performance`.
- [x] Provide `plumos-env` without relying on stock `/mnt/SDCARD/miyoo/lib`.
- [x] Provide static BusyBox and vfat-safe wrapper applets for plumOS.
- [x] Add first-pass helper userland tools such as `ip`, `ss`, and `strace`.
- [x] Smoke-test `ps`, `top`, `df`, `free`, `tar`, `find`, `grep`, `sed`, `awk`, `ip`, `ss`, `lsof`, and `strace` on device.
- [x] Validate the dynamic linker and shared library bundling policy with the SDL2 probe.
- [x] Run a smoke test that uses only the plumOS runtime path.
- [ ] Add `procps-ng`, `coreutils`, `util-linux`, or similar packages if needed.

## Phase 3 - Device Deployment Loop

- [x] Create a deploy script that transfers Docker outputs to the A30.
- [x] Decide the transfer method for SSH/SCP/rsync-style deployment.
- [x] Create helpers for running commands and collecting logs on the device.
- [x] Create a `/dev/fb0` capture helper.
- [x] Standardize display/emulator process shutdown in `scripts/stop-a30-display-processes.sh`.
- [ ] Combine build, deploy, run, and log collection into one command.
- [ ] Document an SD-card-only rollback procedure.

## Phase 4 - Boot Bootstrap

- [x] Create a rollback-capable `MainUI` wrapper.
- [x] Start `plumos-controller-ui-mali` as the normal FE from the wrapper.
- [x] Fall back to stock MainUI if wrapper startup fails.
- [x] Write wrapper and plumOS frontend logs to `/mnt/SDCARD/plumos/logs`.
- [x] Stop stock `keymon` at boot and start the plumOS FE plus network recovery path.
- [x] Stop StockOS `MtpDaemon` / `adbd` / `sysntpd` at boot, then start plumOS-managed NTP after DHCP.
- [x] Expose network recovery from the START menu and Network Settings.
- [x] Confirm Wi-Fi, DHCP, SSH, and FE recovery after A30 power cycling.
- [ ] Decide whether safe OS reboot is possible, since stock MainUI has no reboot item.

## Phase 5 - Frontend Compatibility Layer

- [x] Read stock `Emu`, `RApp`, `App`, and `Themes` `config.json` files as inventory.
- [x] Resolve `rompath`, `imgpath`, `gamelist`, and `launchlist` and check existence/counts.
- [x] Confirm the name matching rules between ROM files and artwork files.
- [x] Detect `extlist`, `launchlist`, and `recentlist` with the compatibility scanner.
- [x] Create a decision-record template for reusing stock behavior.
- [ ] Read the stock data schema for `gamelist` XML.
- [ ] Recognize existing ROMs, artwork, saves, and states without moving them.
- [ ] Implement `extlist` filtering in the frontend ROM list.
- [ ] Support alternate launchers/cores from `launchlist` in UI and launch profiles.
- [ ] Parse and update `Roms/recentlist.json` compatibility data.
- [ ] Reproduce launch compatibility for `EMU_DIR`, `LD_LIBRARY_PATH`, and ROM path `$1`.
- [ ] Move shell launch script behavior into plumOS launch profiles.

## Phase 6 - Frontend Implementation

- [x] Design the plumOS frontend data model for systems, apps, and themes.
- [x] Implement `systems.json` seeding, ROM scanning, on-enter scanning, and directory game scanning.
- [x] Implement the text UI for system, ROM, list, start, settings, core, favorites, recent, and resume views.
- [x] Implement the controller UI for TOP, ROM, START, Settings, Power, HELP, and Core Select views.
- [x] Implement the Mali TTY renderer, rotation auto-detection, A30 compact layout, and blue/orange accents.
- [x] Implement paging, scrolling, key repeat, Japanese text rendering, and marquee behavior for TOP/ROM lists.
- [x] Organize the START menu and settings hierarchy into UI Settings/System/Network/Performance/Apps/HELP/Shutdown.
- [x] Implement UI Settings checkboxes, value selectors, persistence, and runtime application.
- [x] Connect System Settings Volume/Brightness/Lumination/Display Color/Language to plumOS config and runtime backends.
- [x] Store Brightness as `1..20` and map it to RAW lcdbl table `2,3,4,5,6,7,8,9,10,26,43,59,75,92,108,125,141,157,174,190`.
- [x] Hide the brightness tile test screen from normal users behind `PLUMOS_CONTROLLER_BRIGHTNESS_TEST=1`.
- [x] Implement read-only status and network recovery action in Network Settings.
- [x] Implement FTP/SFTP/Samba checkboxes and persistent ON/OFF in Network Settings, then smoke-test them on device.
- [x] Connect Performance Settings to the existing CPU profile/core override features.
- [x] Record A30 UI design rules in `docs/a30-ui-design.md`.
- [ ] Implement Graphic UI mode / gallery mode.
- [x] Design the artwork scraper and thumbnail cache pipeline.
- [x] Implement ROM thumbnail scraping.
- [x] Add FE scraper operation with system selection and progress UI.
- [ ] Add scraper retry UI.
- [ ] Add FE rescue option for filename candidates after CRC miss.
- [ ] Remove horizontal compression from the current TTY prompt rendering.
- [ ] Link brightness hotkey behavior to System Settings.
- [ ] Expand language localization beyond Settings UI strings.
- [ ] Decide Theme selection names, paths, and scope.
- [x] Design a safe Wi-Fi editor flow with backup and rollback.
- [x] Verify Connect Wi-Fi SSID scan, password entry, DHCP, gateway ping, and IP display on the device.
- [x] Measure simultaneous multi-file transfers and throughput for FTP/SFTP/Samba, then update the README recommendation.
- [x] Bundle `dosfsck` / a FAT checker in plumOS userland before making USB Disk Mode a normal feature.
- [x] Validate the USB Disk Mode enter/finish flow on hardware, then settle a safe return path before exposing it in the normal UI.

## Phase 7 - RetroArch, Libretro, and Standalone Runtime

- [x] Build and deploy RetroArch minimal/practical runtime for A30.
- [x] Confirm OSS audio, SDL2 joypad, and FE-owned launch with the RetroArch practical runtime.
- [x] Implement SELECT+B menu, SELECT+START quit, and FE return input drain/cooldown.
- [x] Build/deploy `fceumm` and `gambatte`, then verify NES/GB on the actual screen.
- [x] Build Class A/B libretro cores for A30 armv7 hard-float.
- [x] Pin Virtual Boy `mednafen_vb` to the Onion-era commit and verify Bad Apple on hardware.
- [x] Finalize PPSSPP standalone display, input, menu hotkey, config defaults, and joystickd startup.
- [x] Decide the launch approach for ScummVM, EasyRPG Player, PCSX-ReARMed, and DOSBox-Pure.
- [x] Exclude standalone DOSBox Staging from normal distribution and use DOSBox-Pure as the DOS default candidate.
- [x] Implement CPU restore, joystickd shutdown, framebuffer restore, and FE return in the FE-owned launcher.
- [x] Deploy and validate built Class A/B cores per system.
- [x] Triage problematic emulator/core profiles by symptom, then either fix them or exclude them from normal candidates.
- [ ] Research source provenance for Onion-prebuilt-only cores and add A30-buildable ones to `libretro-core-recipes.tsv`.
- [ ] Revalidate standalone launcher cleanup on normal exit, TERM, KILL, and SSH disconnect.
- [ ] Verify normal user-initiated exits for PPSSPP/PCSX/ScummVM/EasyRPG/RetroArch from the FE A-button path.
- [x] Keep Red Viper standalone for investigation only and remove it from normal distribution and FE profiles.
- [x] Implement backup, restore, and factory reset for RA/PICO/SA emulator settings, including PPSSPP config/controls.
- [ ] Document why the stock PPSSPP landscape menu works compared with official PPSSPP.
- [ ] Manage per-system/core differences with `--config` and override config.

## Phase 8 - A30 Input, Power, and System Policy

- [x] Confirm A30 physical input mapping and `/dev/input/event*` paths.
- [x] Implement `/dev/ttyS0` serial stick frame handling and `plumos-joystickd`.
- [x] Confirm `plumos-joystickd --device-mode xbox` through SDL2, PPSSPP, and RetroArch paths.
- [x] Implement and confirm the power-button Power menu, sleep, and poweroff.
- [x] Measure CPU governor/frequency/core policy and set the FE default to 648MHz / 2 cores.
- [x] Confirm Wi-Fi power sequencing, DHCP, and Dropbear recovery several times.
- [ ] Capture left-stick up/down/left/right individually and finalize X/Y axes and signs.
- [x] Separate the power-button short-press event code from stock sleep/shutdown intervention.
- [x] Make the emulator-runtime power-button UX use the overlay Power menu.
- [ ] Decide whether to keep using stock Wi-Fi userland or move it into plumOS.
- [x] Decide whether SSH remains a development package or becomes a plumOS service.

## Phase 9 - Packaging and Release

- [x] Create the runtime package to extract onto an SD card.
- [x] Create the developer Docker/toolchain package.
- [x] Decide how to separate end-user and developer releases.
- [ ] Summarize the differences between StockOS and plumOS.
- [ ] Summarize plumOS benefits for end users.
- [ ] Document the constraints and tradeoffs when migrating from StockOS to plumOS.
- [x] Organize license notices and upstream attribution.
- [ ] Confirm StockOS SD payload redistribution policy before a public binary release.
- [ ] Confirm NextCommander redistribution terms or replace the file manager before a public binary release.
- [x] Create a GitHub Release package/artifact workflow.
- [ ] Validate install and rollback on a fresh-SD-card-equivalent environment.
