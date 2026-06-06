# plumOS Design Policy

This document records the design direction for plumOS on the Miyoo A30.

## Principles

- Keep the plumOS runtime under `/mnt/SDCARD/plumos`.
- Do not depend on stock `/mnt/SDCARD/miyoo/lib` or `/mnt/SDCARD/RetroArch`.
- Manage the frontend, libraries, RetroArch, libretro cores, configuration,
  cache, and logs from plumOS.
- Do not modify the A30 rootfs/NAND during the early bring-up phase.
- Treat stock files as compatibility input, not as long-term runtime
  dependencies.
- Do not assume the stock behavior is optimal; measure and replace it when a
  better approach is proven.

## Proposed Directory Layout

```text
/mnt/SDCARD/plumos/
  bin/                  frontend, launcher, service commands
  lib/                  plumOS bundled shared libraries
  libexec/              helper programs
  runtime/              dynamic linker/sysroot pieces if needed
  frontend/             frontend assets and modules
  retroarch/
    bin/                RetroArch executable
    cores/              libretro core .so files
    info/               core info files
    assets/             RetroArch assets
    shaders/            shaders if usable on A30
  config/
    retroarch/          retroarch.cfg and overrides
    frontend/           plumOS frontend settings
    systems/            system launch profiles
  state/
    saves/
    states/
    playlists/
    recent/
  cache/
  logs/
  ssh/
```

## Boundary With The Stock Boot Flow

The stock A30 boot flow executes `/mnt/SDCARD/miyoo/app/MainUI` directly. Even
with an independent plumOS runtime, the first boot entry point needs to be one
of these:

- Replace `/mnt/SDCARD/miyoo/app/MainUI` with a small wrapper.
- Eventually replace the rootfs-side `/etc/main` behavior.

For the early phase, use the safer first option. The `MainUI` wrapper should do
only the minimum necessary work, then execute
`/mnt/SDCARD/plumos/bin/plumos-frontend`.

Wrapper requirements:

- Fall back to stock MainUI on failure.
- Be recoverable through SD-card changes only.
- Write logs under `/mnt/SDCARD/plumos/logs`.
- Set the plumOS runtime paths explicitly.

## Runtime Policy

Because plumOS should not reuse stock libraries, binaries should use one of
these strategies:

- Static linking where practical, especially for small utilities.
- A bundled dynamic linker and shared libraries under plumOS when dynamic
  linking is required.

RetroArch, SDL, and other dependency-heavy components should be built against an
explicit A30 sysroot. Since the stock A30 runtime uses glibc `2.23`, do not
assume generic Linux armhf binaries will work.

## RetroArch Policy

- Put RetroArch at `/mnt/SDCARD/plumos/retroarch/bin/retroarch`.
- Put cores under `/mnt/SDCARD/plumos/retroarch/cores`.
- Do not depend on the stock `HOME=/mnt/SDCARD/RetroArch` layout.
- Use `--config` to point RetroArch at a plumOS-managed config.
- Manage per-system differences through launch profiles or override configs.
- Update cores in stages, validating boot, save/state, input, and performance
  before adoption.

During the compatibility phase, a launcher can still read stock `launch` and
`launchlist` entries. Long term, prefer plumOS launch profiles over scattered
shell scripts.

## CPU Policy

Stock launch scripts sometimes set the CPU governor to `performance` and take
CPU2/CPU3 offline. This may be an optimization, but it has not been validated
yet.

plumOS should test:

- All CPU cores online.
- CPU2/CPU3 offline and its effect on heat, battery, and stutter.
- `performance`, `ondemand`, and `interactive` governors.
- Required clocks per system/core.
- Reliable CPU state restoration after game exit.

CPU policy should be centralized in the plumOS launcher, not spread across
per-system scripts.

## Wi-Fi Policy

Wi-Fi involves the kernel driver, power sequencing, `wlan0`, `wpa_supplicant`,
and a DHCP client. The stock init script uses `devmem` for radio power control,
so first observe and reproduce the stock sequence safely.

Goals:

- Put Wi-Fi configuration UI in the plumOS frontend.
- Keep secrets in `/config` or private plumOS config, never in git.
- Bundle userland `wpa_supplicant` and DHCP client if practical.
- Treat kernel modules and radio power sequencing as A30-specific constraints.

## Input Policy

The stock system starts `keymon` as a companion process. Keep it initially while
replacing the frontend, then investigate:

- Reading button state directly from `/dev/input/event*`.
- Whether SDL2 game controller mapping is sufficient.
- Separating RetroArch hotkeys from frontend controls.
- Handling suspend/resume, brightness, and volume keys.

## Stock Behavior To Revisit

Replacement candidates:

- Per-system shell launch scripts.
- RetroArch startup that depends heavily on `HOME` and relative paths.
- Distributed CPU governor/offline/overclock logic.
- Stock `miyoo/lib` dependency.
- Stock `keymon` dependency.
- Stock Wi-Fi userland dependency.

Compatibility to preserve:

- Read existing SD-card `Emu`, `RApp`, `App`, `Roms`, `Imgs`, and `Themes`.
- Recognize existing ROMs and artwork without moving them.
- Migrate existing saves and states.
- Recover through SD-card changes if boot fails.

## Immediate Experiments

1. Build a rollback-safe `MainUI` wrapper.
2. Add `/mnt/SDCARD/plumos/bin/plumos-env` to set runtime paths.
3. Manually run a plumOS frontend prototype while keeping stock MainUI.
4. Run minimal SDL/input/audio/video test binaries on the A30.
5. Build RetroArch `v1.22.2` for the A30 and validate one system first.
6. Collect comparison logs for CPU policies.
