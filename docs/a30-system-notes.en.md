# Miyoo A30 System Notes

Collected from the test unit over SSH on 2026-06-06. Raw artifacts are under
`artifacts/` and are intentionally ignored because they may contain local Wi-Fi
configuration.

## Hardware And OS

- Kernel: Linux `3.4.39`, `armv7l`, `sun8i`.
- CPU: ARMv7, 4 cores, NEON/VFPv4 available.
- RAM: 512 MB.
- Firmware base: Tina/OpenWrt-like system, `Neptune 3.5.1`,
  target `r16-parrot/generic v2.2`.
- libc: glibc `2.23`.
- Main display path logs show `480x640` framebuffer and Mali/GLES use.

## Storage And Mounts

- Root: `/dev/mtdblock3` mounted at `/` as `squashfs`, read-only, 10 MB.
- Config: `/dev/mtdblock2` mounted at `/config` as JFFS2, 384 KB writable.
- SD card: `/dev/mmcblk0p1` mounted at `/mnt/SDCARD` as vfat.
- Runtime: `/tmp`, `/run`, and `/dev` are tmpfs; `/var` points to `/tmp`.

Implication: first-stage CFW work should stay SD-card based. Rootfs changes
require repacking/flashing squashfs and should be deferred until the boot flow
is fully understood.

## Boot Flow

1. Kernel command line uses `root=/dev/mtdblock3`, `rootfstype=squashfs`,
   `init=/sbin/init`, and `rdinit=/rdinit`.
2. `/sbin/procd` is PID 1.
3. `/etc/inittab` runs `/etc/init.d/rcS S boot`.
4. `rc.d` starts normal OpenWrt-style services, including `network`,
   `wpa_supplicant`, `sysntpd`, `adbd`, `mtp`, and `done`.
5. `/etc/init.d/done` runs `/etc/rc.local`.
6. `/etc/rc.local` redirects output to `/tmp/rc.local.log` and starts
   `/etc/main &`.
7. `/etc/main` sets Miyoo paths, configures audio, mounts `/config` if needed,
   handles update packages, starts `keymon`, then launches the frontend.

Frontend launch priority in `/etc/main`:

- If `/mnt/SDCARD/miyoo/` exists:
  - `LD_LIBRARY_PATH=/lib:/usr/lib:/mnt/SDCARD/miyoo/lib`
  - starts `/mnt/SDCARD/miyoo/app/keymon`
  - `cd /mnt/SDCARD/miyoo/app`
  - runs `./MainUI`
- Otherwise it falls back to `/usr/miyoo/bin/keymon` and `/usr/miyoo/bin/MainUI`.

Implication: the safest replacement point is SD-card
`/mnt/SDCARD/miyoo/app/MainUI`, ideally with a wrapper/rollback plan before
replacing the binary outright.

## Wi-Fi

- Service: `/etc/init.d/wpa_supplicant`, started as `S96wpa_supplicant`.
- The init script toggles Wi-Fi power via `devmem`, brings up `wlan0`, then runs:
  `wpa_supplicant -B -D nl80211 -iwlan0 -c /config/wpa_supplicant.conf`.
- DHCP is handled by `udhcpc -i wlan0`.
- Runtime state lands in `/tmp/wpa_status.txt`, `/tmp/.wpa2_log`, and
  `/proc/net/wireless`.

Implication: a replacement frontend can manage Wi-Fi by editing
`/config/wpa_supplicant.conf` and restarting/controlling the existing service,
but should preserve the stock power-up sequence.

## Current Frontend

- Running binary: `/mnt/SDCARD/miyoo/app/MainUI`.
- Working directory: `/mnt/SDCARD/miyoo/app`.
- Companion process: `/mnt/SDCARD/miyoo/app/keymon`.
- Runtime footprint observed: about 21 MB RSS, 12 threads.
- User/system settings: `/config/system.json`.
- Theme font/color settings: `/mnt/SDCARD/miyoo/res/config.json`.
- Extra platform/gamelist mapping: `/mnt/SDCARD/miyoo/app/oplatform.json`.
- Recent list: `/mnt/SDCARD/Roms/recentlist.json`.

Important settings observed in `/config/system.json`:

- volume, mute, background music volume
- brightness and display color values
- language
- theme path
- keymap
- Wi-Fi enabled flag
- CPU frequency mode flag

Current values confirmed on 2026-06-06:

- `vol=14`, `mute=1`, `bgmvol=13`
- `brightness=10`, `lumination=5`
- `contrast=10`, `hue=10`, `saturation=10`
- `wifi=1`, `cpufreq=0`
- `keymap=L,L2,R,R2,X,A,B,Y`
- `language=en.lang`

Backend candidates:

- Volume: `amixer` exists, but mixer control mapping is not validated.
- Brightness: `/sys/class/backlight` does not expose usable brightness files.
- Wi-Fi runtime: redacted status is readable from `/tmp/wpa_status.txt`.
- Input: `gpio-keys-polled` normally resolves to `/dev/input/event3`.

Detailed UI policy lives in [A30 settings UI policy](a30-settings-policy.en.md).

## Runtime Probe

On 2026-06-06, `plumos-runtime-probe` was run on the A30 and confirmed:

- Video: `/dev/fb0` reports `480x640`, `32bpp`, line length `1920`.
- Video write: a small 64x64 patch can be drawn briefly and restored.
- Input: `gpio-keys-polled` opens and polls as `/dev/input/event3`.
- Audio: `/dev/dsp` exists, but is busy while stock `MainUI` holds
  `/dev/snd/pcmC0D0p`.
- SDL2: plumOS-bundled upstream SDL3 3.4.10 + sdl2-compat 2.32.68 plus bundled
  dynamic loader/shared libraries starts a linked/window/input probe and
  automatically recognizes the composite virtual pad as a GameController. The
  real framebuffer/render backend is still unvalidated.

Details live in [A30 runtime probe](a30-runtime-probe.en.md).

## Input Policy

On 2026-06-06, `plumos-input-compare` was run on the A30 and confirmed:

- Stock `keymon` opens `event0`, `event1`, `event2`, and `event3`.
- Stock `MainUI` also opens `event0`, `event1`, and `event3`.
- `gpio-keys-polled` is `/dev/input/event3`.
- Even while `keymon` and `MainUI` are running, plumOS can open and poll
  `/dev/input/event3` directly.
- Physical button mapping is confirmed for every button except power:
  A=`KEY_SPACE`, B=`KEY_LEFTCTRL`, START=`KEY_ENTER`,
  SELECT=`KEY_RIGHTCTRL`, Function=`KEY_ESC`.
- Left stick axes were not observed as `EV_ABS` under `/dev/input/event*`.
  MainUI calibration updates `/config/joypad.config`.
- The spruceOS A30 implementation converts `/dev/ttyS2` raw serial data into
  virtual input through `joystickinput`. On the observed stock A30, `/dev/ttyS2`
  is not created initially, but the `ttyS` driver exposes minors 0-4.
- On the device observed on 2026-06-06, `/dev/ttyS0` produced 9600/8N1 joystick
  frames in the form `ff b1 b2 b3 b4 fe`.
- The minimal `plumos-joystickd` implementation can create a
  `plumOS A30 Analog Stick` (`js0`/`event4`) device through `/dev/uinput`.
- `plumos-joystick-reader` confirmed that the `js0`/`event4` device created by
  `plumos-joystickd` can be read through both the Linux joystick API and evdev.
- The left-stick click was not observed through normal kernel input events or
  the `/dev/ttyS0` 6-byte serial frame.
- Observed stockOS/spruceOS analog-stick calibration storage only contains X/Y
  axis min/max/center values, with no stick-click setting. Initial plumOS treats
  the left-stick click as unconnected/unsupported.
- Stock RetroArch has SDL1 enabled and SDL2/udev/evdev disabled. The axes-only
  `js0` and composite `plumOS A30 Gamepad` from `plumos-joystickd` are visible
  through the Linux joystick API, but stock RetroArch logs did not confirm
  autoconfig/connection.
- In stock RetroArch launched from MainUI, the Port 1 Controls binding UI
  detects the left stick as `Axis -2`/`+/-2`, but the stick does not move the
  menu cursor in normal operation.
- Stock PPSSPP starts `miyoo282_xpad_inputd` from `launch.sh`; that daemon uses
  `/dev/ttyS0` and `/config/joypad.config` to create `MIYOO Pad1`
  (`045e:028e`, `js0`/`event4`, 8 axes/11 buttons) through `/dev/uinput`.
- PPSSPP itself reads `MIYOO Pad1` through `libSDL2-2.0.so.0` and
  `SDL_GameController*` / `SDL_Joystick*` APIs. The left-stick click also did
  not react in PPSSPP controller settings.
- When PPSSPP was launched directly without stock `miyoo282_xpad_inputd`,
  `plumos-joystickd --device-mode xbox` created `plumOS A30 Gamepad`, which was
  assigned to pad 1 after a successful SDL2 GameController mapping.
- For RetroArch and standalone emulator analog input, prioritize testing a
  `plumos-joystickd` buttons+axes composite virtual pad mode plus SDL2/evdev in
  the plumOS RetroArch build instead of relying on the stock SDL1 path.
- Short stock MainUI/keymon, PPSSPP direct-launch, and stock RetroArch probes
  left no stale `plumos-joystickd --device-mode xbox` process/device/fd behind.
- `plumos-joystickd --device-mode xbox` button forwarding was confirmed for
  A/B/X/Y, D-pad, L/R, L2/R2, START/SELECT, and Function as
  `plumOS A30 Gamepad` button/hat/trigger events.
- With the plumOS-bundled upstream SDL3 3.4.10 + sdl2-compat 2.32.68 probe, the
  `plumos-joystickd --device-mode xbox` composite virtual pad was also
  auto-detected through the `Xbox 360 Controller` mapping.
- Because the power button may be handled on the kernel side, use Function as
  the primary candidate for the safe shutdown/resume menu.

For now, keep stock `keymon` while the plumOS frontend reads
`/dev/input/event3` directly. Details live in
[A30 input policy](a30-input-policy.en.md).

## Frontend Data Model To Preserve

The stock UI reads SD-card content from these families:

- `/mnt/SDCARD/Emu/*/config.json`: primary emulator categories.
- `/mnt/SDCARD/RApp/*/config.json`: additional or alternate libretro cores.
- `/mnt/SDCARD/App/*/config.json`: apps/tools.
- `/mnt/SDCARD/Themes/*/config.json`: theme entries.
- `/mnt/SDCARD/Roms/*`: ROM folders.
- `/mnt/SDCARD/Imgs/*`: artwork folders.

Observed config fields:

- `label`
- `icon`
- `iconsel`
- `launch`
- `rompath`
- `imgpath`
- `gamelist`
- `useswap`
- `shortname`
- `hidebios`
- `extlist`
- `launchlist`
- `description`

Compatibility requirements:

- Enumerate systems/apps from JSON, not hard-coded lists.
- Filter ROMs by `extlist`.
- Respect alternate launchers in `launchlist`.
- Pass the selected ROM path to launch scripts as `$1`.
- Set `EMU_DIR` to the emulator directory before running launch scripts; some
  stock scripts rely on `$EMU_DIR/overclock` without defining `EMU_DIR`.
- Preserve `Roms/recentlist.json` behavior.
- Preserve artwork lookup through `imgpath`.
- Preserve theme discovery and font/color metadata.

## RetroArch And Cores

- Current RetroArch version: `1.16.0`.
- `retroarch` build string: `MIYOO Build: Apr 25 2024 22:32:35`.
- `ra32.miyoo` build string: `MIYOO Build: Apr 4 2024 15:25:23`.
- Both are 32-bit and need SD libraries such as `libSDL-1.2.so.0` from
  `/mnt/SDCARD/miyoo/lib`.
- Core directory: `/mnt/SDCARD/RetroArch/.retroarch/cores`.
- Observed installed core `.so` count: 73.
- Core directory size: about 908 MB.
- RetroArch config uses:
  - `audio_driver = "sdl"`
  - `input_driver = "sdl"`
  - `input_joypad_driver = "sdl"`
  - `libretro_directory = "./.retroarch/cores"`
  - `system_directory = "./.retroarch/system"`
  - `savefile_directory = "./.retroarch/saves"`
  - `savestate_directory = "./.retroarch/states"`

Before building RetroArch, re-check the official upstream latest stable release
and select the candidate at build time. Because A30 uses old glibc `2.23`, we
should assume we need our own armv7 hard-float build rather than relying on
current generic buildbot binaries. Treat stockOS RetroArch/core versions as
compatibility references only, and compare against them only when newer builds
show hardware problems.

## Launch Script Patterns

Most emulator scripts:

- Set CPU governor to `performance`.
- Often offline CPU2 and CPU3 before launch.
- Use per-system `overclock` helpers with values such as `648`, `816`, `1200`,
  or `1512`.
- Start RetroArch with:
  `HOME=/mnt/SDCARD/RetroArch/ retroarch -v -L CORE_PATH ROM_PATH`
  or:
  `HOME=/mnt/SDCARD/RetroArch/ ra32.miyoo -v -L CORE_PATH ROM_PATH`.

Notable exceptions:

- PPSSPP has a standalone `PPSSPPSDL` path and `miyoo282_xpad_inputd`, which
  creates an Xbox 360-like `MIYOO Pad1` virtual pad read through SDL2.
- Some arcade/shooting paths use `ra32.miyoo` with `"$*"`.
- PS launch scripts copy per-game BIOS files into RetroArch system paths.

## Stock Behavior To Re-Evaluate

The stock behavior is important because it works, but plumOS should not treat it
as automatically optimal. Items to verify before copying the same design:

- Launch behavior is spread across per-system shell scripts, with incomplete
  environment and CPU state restoration.
- RetroArch startup depends heavily on `HOME` and relative paths.
- Taking CPU2/CPU3 offline may or may not improve performance, heat, battery,
  or stability.
- Per-system overclock values need measurement.
- Stock `keymon` may be replaceable by direct input event handling or SDL2
  controller mapping.
- Wi-Fi userland may be movable into plumOS after the A30-specific power
  sequence is understood.

See [plumOS design policy](plumos-design-policy.en.md) for the current design
direction.

## Next Decisions

- Build a minimal SD-card frontend replacement under `/mnt/SDCARD/plumos`
  first, then use a wrapper at `/mnt/SDCARD/miyoo/app/MainUI` only after a
  tested rollback path exists.
- Keep the wrapper thin; the actual frontend, libraries, RetroArch, cores,
  configs, and logs should live under `/mnt/SDCARD/plumos`.
- Avoid depending on stock `/mnt/SDCARD/miyoo/lib` for the final runtime.
- Implement a compatibility launcher that mirrors MainUI's environment:
  working directory, `LD_LIBRARY_PATH`, `EMU_DIR`, ROM argument passing, and
  recent-list updates.
- Keep stock `keymon` initially unless input behavior requires replacement.
- Check the latest stable RetroArch and required core releases at build time,
  then build them for `armv7-a hard-float`, targeting glibc 2.23 or a
  compatible static/dynamic strategy.
- Update cores in groups, validating ABI and performance per core rather than
  replacing all 908 MB at once.
