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
- [ ] For SDL2, RetroArch, libretro cores, standalone emulators, and helper
  libraries, check the latest stable upstream release at build time and record
  the selected version/tag/commit/build options in manifests.
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
- [x] Validate the bundled dynamic linker/shared library strategy with the SDL2
  probe when dynamic linking is required.
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
- [x] Stop stock `keymon` from the wrapper launched by stock `/etc/main`, and
  automatically run the plumOS network rescue helper during boot.
- [x] Add controller UI `--rescue-network` for reboot recovery, so A reruns the
  Wi-Fi start path, DHCP, and SSH start.
- [x] Confirm the A30 remains recoverable through network rescue after a power
  cycle.
- [ ] Since stock MainUI has no reboot item, investigate why the `reboot` command
  goes dark/LED-off without reaching the Miyoo logo, and decide whether safe OS
  reboot is supported.

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
- [x] Create the initial `systems.json` seed.
- [x] Build a ROM scan prototype using Miyoo/ROCKNIX directory aliases.
- [x] Build an on-enter per-system ROM directory re-scan prototype.
- [x] Measure first text-mode display time with 1000 dummy ROM files.
- [x] Build an SSH-checkable text-mode system list / ROM list prototype.
- [x] Implement a START menu UI model for settings/apps/network/shutdown.
- [x] Implement system default core selection with SELECT on a highlighted TOP
  system.
- [x] Implement per-ROM core override selection with SELECT on a highlighted ROM
  entry.
- [x] Implement core priority as ROM override > system default > plumOS
  recommended > auto detect.
- [x] Store core selection as an extensible launch profile id, not only a core
  id.
- [x] Implement favorite toggle in the ROM list and open Favorites from the
  START menu.
- [x] Add an optional setting to show Favorites as a virtual TOP system.
- [x] Manage recent ROM/launch profile/resume availability in `recent.json`.
- [x] Manage auto-resume target ROM/core/launch profile/pending state in
  `resume-session.json`.
- [x] Implement a text UI prototype for `boot_resume_mode=off|last|picker`.
- [x] Build a launch plan for the same ROM/core/launch profile on boot, and
  auto-launch it with `boot --execute` when a pending resume session exists.
- [x] Build a minimal controller-first frontend prototype.
- [x] Implement system list, ROM list, recents, favorites, and settings screens
  in the controller UI.
- [x] Decide how to load themes, fonts, and artwork.
- [x] Implement thumbnail lookup that preserves paths relative to the ROM alias
  root.
- [x] Implement thumbnail lookup with subdirectory priority, flat fallback, and
  placeholder fallback.
- [x] Design a plumOS theme model that separates theme, layout preset, and
  frontend behavior.
- [x] Define the schema for `/mnt/SDCARD/plumos/themes/<theme_id>/theme.json`.
- [x] Limit themes to colors, fonts, backgrounds, system icons, selection style,
  spacing, thumbnail frames, and sound effects.
- [x] Prevent themes from changing behavior such as button mappings, START menu,
  SELECT core menu, favorites, ROM scanning, and resume.
- [x] Implement a text-mode fallback that remains usable with default font/color
  even when theme assets or fonts are missing/broken.
- [x] Keep stock theme compatibility out of the initial spec; consider it later
  only as an importer if needed.
- [x] Decide how to handle A30 settings such as brightness, volume, Wi-Fi, and
  keymap.
- [ ] Implement write-enabled A30 settings controls only after backend
  validation, backup, atomic write, and `sync` policy are settled.
- [x] Run a minimal framebuffer/input/audio runtime probe binary on the A30.
- [x] Run a minimal linked/window/input probe binary with plumOS-bundled SDL2 on
  the A30.
- [x] Validate the plumOS-bundled SDL2 framebuffer/render options on the A30 and
  confirm that upstream SDL3+sdl2-compat only reaches dummy/offscreen/evdev
  software rendering without a real display backend.
- [x] Inspect stock SDL1/SDL2 video backends and confirm that stock SDL2 uses a
  custom `mali` backend for `/dev/fb0` + Mali fbdev EGL.
- [x] Implement a clean-room fbdev + Mali EGL presenter probe without linking to
  stock SDL and confirm `eglSwapBuffers` on the A30.
- [x] Choose the initial real-display path: integrate fbdev + Mali EGL directly
  into the frontend first, and confirm TOP display plus ROM-list navigation with
  `plumos-controller-ui-mali` on the A30.
- [x] Convert `plumos-controller-ui-mali` to an A30-oriented compact layout and
  confirm TOP/ROM/Settings/SAFE exercise plus a 30-second hold while stock
  MainUI/keymon are still present.
- [x] Add `--rotation auto|none|cw|ccw` to `plumos-controller-ui-mali`; on the
  A30 `480x640` framebuffer, `auto` draws the landscape UI in the same raw
  orientation as stock.
- [x] Confirm that Wi-Fi/SSH stay up through `wpa_supplicant`/`udhcpc`/`dropbear`
  rather than stock `MainUI.stock`/`keymon`.
- [x] Confirm that OS boot starts Wi-Fi through `/etc/rc.d/S96wpa_supplicant`
  -> `/etc/init.d/wpa_supplicant`.
- [x] Treat IP acquisition/DHCP as likely MainUI-dependent and automatically run
  `plumos-network-rescue` when the FE starts.
- [x] Run the same network rescue path from START menu Network and from the
  Settings `A30 Wi-Fi Config`/`A30 Wi-Fi Runtime` rows.
- [x] Use
  `scripts/probe-a30-frontend-mali.sh --stop-mainui --stop-keymon --no-restart-stock`
  to validate the Mali UI in the plumOS target state with stock `/etc/main`,
  `MainUI.stock`, and `keymon` stopped.
- [ ] Visually inspect `plumos-controller-ui-mali` on the device screen and tune
  text readability, spacing, colors, and selection display.
- [ ] Have the user visually confirm the physical screen orientation for
  `--rotation auto`; switch to `cw`/`ccw` if it is reversed.
- [ ] Revisit SDL3/sdl2-compat custom video backend work after the frontend
  presenter behavior is stable.
- [ ] Re-test audio playback with stock MainUI stopped or replaced.
- [x] Compare keeping stock `keymon` with reading `/dev/input/event*`
  directly.
- [x] Confirm physical button code/action mapping except power and left-stick
  click with
  `plumos-input-compare --all-events`.
- [x] Add `EV_ABS` output to `plumos-input-compare` and check whether the left
  stick is exposed as kernel input.
- [x] Check the left-stick click with `plumos-input-compare --all-events` and
  `/dev/ttyS0` serial frames; record that it does not appear in normal kernel
  input or the 6-byte serial frame.
- [x] Based on stockOS/spruceOS calibration storage and hardware event results,
  treat the left-stick click as unconnected/unsupported in initial plumOS.
- [x] Inspect the spruceOS A30 implementation and confirm that `joystickinput`
  uses `/dev/ttyS2` raw serial data plus `/config/joypad.config`.
- [x] Add `plumos-serial-joy-probe` for checking raw `/dev/ttyS*` data.
- [x] Confirm that the A30 emits 9600/8N1 `ff b1 b2 b3 b4 fe` frames from
  `/dev/ttyS0`.
- [x] Capture serial frame ranges while moving the left stick and confirm that
  `axisYR`/`axisXR` are close to the `/config/joypad.config` min/max values.
- [ ] Capture up/down/left/right separately and confirm the X/Y assignment and
  sign for `axisYR`/`axisXR`.
- [x] Record a minimal plumOS `plumos-joystickd` design around
  `/config/joypad.config` and the `/dev/ttyS0` raw serial path.
- [x] Add a minimal `plumos-joystickd` implementation that converts `/dev/ttyS0`
  raw frames into virtual `/dev/uinput` `ABS_X`/`ABS_Y` events.
- [x] Validate that the `plumos-joystickd` virtual input device appears on the
  A30 as `js0`/`event4`.
- [x] Add `plumos-joystick-reader` so the virtual input from
  `plumos-joystickd` can be monitored through the Linux joystick API and evdev.
- [x] Run `plumos-joystick-reader` on the A30 and confirm that `js0`/`event4`
  can be read through the Linux joystick API and evdev.
- [x] Remove the provisional `BTN_THUMBL` from the `plumos-joystickd` virtual
  device because the left-stick click is treated as unsupported.
- [x] Add `js0` device name/axes/buttons output to `plumos-joystick-reader`.
- [x] Check stock RetroArch build features and record that SDL1 is enabled while
  SDL2 and udev/evdev are disabled.
- [x] Add `scripts/probe-a30-retroarch-joystick.sh` so the stock RetroArch SDL
  joystick log probe can be repeated while `plumos-joystickd` is running.
- [x] Record that the stock RetroArch log probe reaches the SDL joypad driver,
  but does not show autoconfig/connection logs for `plumOS A30 Analog Stick`.
- [x] Record that stock RetroArch detects the left stick as `Axis -2`/`+/-2`
  during Port 1 Controls binding, but the stick does not move the menu cursor.
- [x] Confirm that stock PPSSPP starts `miyoo282_xpad_inputd`, which uses
  `/dev/ttyS0`, `/config/joypad.config`, and `/dev/uinput` to create
  `MIYOO Pad1`.
- [x] Add `scripts/probe-a30-ppsspp-input.sh` so the PPSSPP analog input path
  can be re-inspected while PPSSPP is running.
- [x] Document that stock PPSSPP reads an Xbox 360-like `MIYOO Pad1`
  (`045e:028e`, `js0`/`event4`, 8 axes/11 buttons) through SDL2
  GameController/Joystick APIs.
- [x] Verify that with stock RetroArch/SDL1, `plumos-joystickd` axes-only and
  composite virtual input devices are visible through the Linux joystick API,
  but RetroArch logs still do not confirm device/autoconfig recognition.
- [x] Choose the RetroArch analog strategy: do not rely on stock SDL1; prioritize
  SDL2/evdev plus a buttons+axes composite virtual pad in the plumOS RetroArch
  build.
- [x] Add a PPSSPP-like buttons+axes composite virtual pad mode to
  `plumos-joystickd`.
- [x] Add `scripts/probe-a30-joystickd-xbox.sh` so `--device-mode xbox` hardware
  checks can be repeated.
- [x] Verify that `plumos-joystickd --device-mode xbox` appears on the A30 as
  an 8-axis / 11-button `js*`/`event*` device.
- [x] Add `scripts/probe-a30-ppsspp-plumos-gamepad.sh` so stock PPSSPP can be
  tested with the plumOS gamepad and without the stock input daemon.
- [x] Verify that `plumos-joystickd --device-mode xbox` is recognized by
  PPSSPP/SDL2 GameController and loads a GameController mapping successfully.
- [x] Confirm that short stock MainUI/keymon, PPSSPP direct-launch, and stock
  RetroArch probes leave no stale `plumos-joystickd --device-mode xbox`
  process/device/fd behind.
- [x] Add `scripts/probe-a30-joystickd-buttons.sh` and confirm that A/B/X/Y,
  D-pad, L/R, L2/R2, START/SELECT, and Function are forwarded as
  `plumOS A30 Gamepad` button/hat/trigger events.
- [x] Add `scripts/probe-a30-sdl2-gamepad.sh` and confirm that plumOS-bundled
  upstream SDL3 3.4.10 + sdl2-compat 2.32.68 automatically recognizes the
  `plumos-joystickd --device-mode xbox` composite virtual pad as a
  GameController.
- [ ] Check whether keeping `plumos-joystickd --device-mode xbox` always running
  during plumOS causes duplicate input or stale fd issues in the frontend or
  emulators. For comparison probes, also watch the case where stock `keymon`
  keeps deleted `js*`/`event*` fds open.
- [ ] For the plumOS RetroArch build, prioritize testing SDL2/evdev plus a
  composite virtual pad instead of relying on the stock SDL1 path.
- [ ] Lower the priority of the `/dev/mem` ADC path unless later evidence shows
  it is required beyond stock calibration/test screens.
- [x] Implement a controller UI SAFE menu prototype opened by Function.
- [ ] Safely confirm the short power-button event code and stock sleep/shutdown
  side effects.
- [ ] Validate a Function-button fallback for the safe shutdown/resume menu
  while RetroArch is running.
- [x] Decide to stop stock `keymon` when plumOS becomes the regular boot
  frontend.
- [ ] Investigate how to show a Sleep/Shutdown/Cancel menu from Function, or
  from power if available, while RetroArch is running.

## Phase 7 - RetroArch and Core Runtime

- [ ] Check the latest stable RetroArch release at build time and build/test it
  for the A30 armv7 hard-float environment.
- [ ] Place RetroArch at `/mnt/SDCARD/plumos/retroarch/bin/retroarch`.
- [ ] Place cores under `/mnt/SDCARD/plumos/retroarch/cores`.
- [ ] Stop depending on `HOME=/mnt/SDCARD/RetroArch`.
- [ ] Manage per-system/core differences through `--config` and override configs.
- [ ] Update libretro cores per system in stages.
- [ ] Treat upstream latest stable libretro cores and standalone emulators as
  the initial candidates; compare against stockOS versions/patches/build
  options only when hardware testing shows problems.
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
- [x] Trace boot-time `wpa_supplicant`, DHCP, and IP acquisition timing through
  `network-rescue.log` after poweroff -> power on.
- [ ] Repeat poweroff -> power-on Wi-Fi auto-connect checks and tune DHCP retry
  count/timing.
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
