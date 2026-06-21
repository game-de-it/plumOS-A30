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
- Do not automatically adopt stock `config.json`, `launch.sh`, CPU policy, or
  RetroArch startup behavior as plumOS specifications.
- Before directly reusing a stock behavior, document the reason and
  alternatives, then ask the user for confirmation before implementation.
- For plumOS libraries, standalone emulators, and RetroArch, check the upstream
  latest stable release at build time and treat that as the default candidate.
- For libretro cores, import the cores Onion carries and prefer Onion's proven
  version/commit/build recipe when one exists. plumOS-only cores that Onion does
  not carry may still use upstream latest/HEAD as candidates.
- Treat stockOS library/emulator versions as compatibility reference points, not
  normal pinning targets. Only consider matching stockOS versions or patch
  levels when hardware validation shows breakage, performance regressions, or
  A30-specific issues.
- Build primarily inside Docker, transfer artifacts to the device, and validate
  them on real hardware.
- Include developer-facing Docker toolchain files as part of the final project
  deliverables, alongside the end-user runtime package.

## Proposed Directory Layout

```text
/mnt/SDCARD/plumos/
  bin/                  frontend, launcher, service commands
  lib/                  plumOS bundled shared libraries
  libexec/              helper programs
  runtime/              dynamic linker/sysroot pieces if needed
  gnu/bin/              GNU/procps/util-linux style command wrappers
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

## Docker/toolchain Policy

plumOS builds should be reproducible inside Docker instead of depending on the
workstation's local environment. The A30 toolchain, sysroot, build dependencies,
and RetroArch/core build helpers can be customized specifically for plumOS.

Expected repository layout:

```text
docker/
  plumos-toolchain/
    Dockerfile
    README.md
    scripts/
scripts/
  docker-build.sh
  deploy-a30.sh
  run-a30.sh
  collect-a30-logs.sh
  package-release.sh
```

Basic build/deploy loop:

1. Build the Docker image.
2. Build the frontend, helpers, RetroArch, cores, and runtime package inside the
   container.
3. Write outputs to `dist/` or a staging directory.
4. Transfer artifacts to `/mnt/SDCARD/plumos` on the A30 through SSH/SCP/rsync
   or an equivalent path.
5. Run commands on the device.
6. Collect logs/results and feed them into the next iteration.

Docker build targets:

- A30 sysroot/toolchain.
- Framebuffer/input/audio runtime probe.
- plumOS-bundled SDL2 test binary.
- plumOS userland command package.
- plumOS frontend/helpers.
- RetroArch.
- libretro cores.
- Packaging/release tooling.

The Docker image may become large, so git should contain Dockerfiles, scripts,
locks/hashes, patches, and build recipes. Large build caches and generated
binary archives should be treated as release artifacts and split across GitHub
Release assets when needed.

## Upstream Version Policy

The goal is not to reproduce the stockOS runtime stack. plumOS should use newer
upstream components wherever they remain stable on the A30. For libretro cores,
prefer the Onion-proven core catalog and version/commit choices when Onion
carries that core; plumOS-only cores can still start from upstream latest/HEAD.

- Before building SDL2, RetroArch, standalone emulators, or helper libraries,
  check the latest stable upstream release available at that time.
- For libretro cores, import Onion-adopted cores and align them with Onion's
  core names, source provenance, and proven commit or builder recipe. Treat
  recipes without exact provenance as provisional until an A30 build and
  hardware validation prove them.
- For plumOS-only cores that Onion does not carry, use upstream latest/HEAD as
  the normal candidate and pin only when hardware testing shows a regression.
- If a known regression exists, use a fixed commit or the nearest stable release
  instead of blindly taking the latest tag.
- If the A30 kernel, GPU/framebuffer, audio, input, or runtime stack exposes an
  issue, compare against stockOS versions, patches, and build options as
  debugging references.
- Match a stockOS version only when hardware validation shows a real problem
  with the newer build and version difference is a strong cause candidate.
- Record the selected version, source URL, commit/tag, important build options,
  and differences from stockOS in docs or manifests.

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

The runtime package should be produced by Docker builds and should run from
`/mnt/SDCARD/plumos` after extraction to the SD card.

## Userland Command Policy

The stock A30 BusyBox has quirks around options and commands such as `tar`,
`ps`, and `top`. To make development more predictable, plumOS should bundle its
own command set.

Stages:

1. Put a statically linked BusyBox at `/mnt/SDCARD/plumos/bin/busybox`.
2. Install applets as wrapper scripts instead of symlinks so they work on vfat.
3. Use `plumos-env` to put `/mnt/SDCARD/plumos/gnu/bin` and
   `/mnt/SDCARD/plumos/bin` before the stock PATH.
4. Replace commands where BusyBox compatibility is not enough with
   `procps-ng`, `coreutils`, `util-linux`, and similar GNU/Linux userland tools.

High-priority commands include `ps`, `top`, `df`, `free`, `tar`, `find`, `grep`,
`sed`, `awk`, `ip`, `ss`, `lsof`, and `strace`.

The goal is a Debian-like command experience from the SD-card plumOS runtime
without changing the rootfs.

## RetroArch Policy

- Put RetroArch at `/mnt/SDCARD/plumos/retroarch/bin/retroarch`.
- Put cores under `/mnt/SDCARD/plumos/retroarch/cores`.
- Check the upstream latest stable RetroArch release at build time; do not match
  stockOS RetroArch unless A30 hardware validation gives a reason.
- For libretro cores, prefer Onion's adopted catalog and proven
  version/commit/build recipes. Cores that Onion does not carry may remain
  plumOS-only latest-upstream candidates.
- Do not depend on the stock `HOME=/mnt/SDCARD/RetroArch` layout.
- Use `--config` to point RetroArch at a plumOS-managed config.
- Manage per-system differences through launch profiles or override configs.
- Update cores in stages, validating boot, save/state, input, and performance
  before adoption.

During the compatibility phase, a launcher can still read stock `launch` and
`launchlist` entries. Long term, prefer plumOS launch profiles over scattered
shell scripts.

## Frontend Policy

Stock `config.json` files are stock MainUI menu/ROM/artwork/launch definitions;
they are not the official plumOS frontend specification. Details live in
[Stock frontend inventory](stock-frontend-inventory.md).

plumOS should design these pieces independently:

- System/app/theme data model.
- ROM list and artwork lookup.
- Recent/favorite state format.
- Connection to RetroArch/core launch profiles.
- Boundaries with CPU/input/audio/video policy.

Existing SD-card data still needs to be readable, but only as input for
migration, inventory, or compatibility shims. Promoting stock behavior into the
new UI or launcher specification requires an explicit decision.

The native model is documented in
[plumOS frontend data model](frontend-data-model.md).
Use the [Stock reuse decision template](decisions/stock-reuse-template.md)
for decision records.

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
replacing the frontend. On 2026-06-06, `plumos-input-compare` confirmed that
plumOS can open and poll `/dev/input/event3` non-exclusively even while `keymon`
and stock `MainUI` also have it open.

- Read `/dev/input/event3` directly in the plumOS frontend.
- Avoid exclusive grabs while coexisting with stock MainUI.
- Confirm physical button code/action mapping on hardware.
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

1. Build the minimal Dockerfile for the plumOS toolchain.
2. Add a deploy helper that transfers Docker build output to the device.
3. Build a rollback-safe `MainUI` wrapper.
4. Add `/mnt/SDCARD/plumos/bin/plumos-env` to set runtime paths.
5. Manually run a plumOS frontend prototype while keeping stock MainUI.
6. Run a minimal framebuffer/input/audio runtime probe on the A30.
7. Run a minimal linked/window/input probe with plumOS-bundled SDL2 on the A30.
8. Validate the framebuffer/render backend for plumOS-bundled SDL2 on the A30.
9. Check the latest stable RetroArch release at build time, build it for the
   A30, and validate one system first.
10. Collect comparison logs for CPU policies.
