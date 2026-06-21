# RetroArch Current Working Baseline

This document fixes the RetroArch runtime that was confirmed on 2026-06-11 as
not final, but correctly working for the important paths at that time. Use it as
a recovery comparison point for future RetroArch regressions.

Japanese counterpart: [retroarch-known-working-baseline.ja.md](retroarch-known-working-baseline.ja.md)

## Backup

- Backup ID: `retroarch-known-working-20260611-070551`
- A30 backup: `/mnt/SDCARD/plumos/backups/retroarch-known-working-20260611-070551`
- Local artifact:
  `artifacts/a30-backups/retroarch-known-working-20260611-070551/retroarch-known-working-20260611-070551.tar`
- Local artifact SHA256:
  `c718a3432c24dce3d1c4285de0ba7fd91a0929a7d94917e5f315e687c053761b`
- Manifest:
  `artifacts/a30-backups/retroarch-known-working-20260611-070551/MANIFEST.txt`

ROM files are not part of this backup. The snapshot covers the plumOS RetroArch
runtime and launch/profile files needed to reproduce the then-working state.

## Status

Unlike the PPSSPP known-good baseline, this is not a final RetroArch baseline.
It records the state where `fceumm`, `gambatte`, the practical runtime
audio/input path, FE-owned RetroArch launch, and safe exit paths were working.

The 41 cores were not all validated for per-system boot, performance, audio,
input, or save/state behavior at that point. If RetroArch regresses, compare
against this snapshot as the known return point.

## Backup Scope

- `/mnt/SDCARD/plumos/retroarch/`
  - RetroArch binary, config, cores, info, autoconfig, saves/states/logs.
  - Captured core count: `41`
- `/mnt/SDCARD/plumos/bin/plumos-retroarch-launch`
  - Launcher that assembles config, core, CPU, audio/input, and safe exit.
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - A30 virtual gamepad used by the RetroArch SDL2 joypad path.
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - Source data for FE-selected RetroArch launch profiles.
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - System/ROM-specific core, CPU, audio, and other override state.

## Key Hashes

- `retroarch/bin/retroarch`:
  `b12efdb65a13d08eab8452571694c01815a68c4af5941873887d7b5c38b65642`
- `retroarch/bin/retroarch.bin`:
  `f4c0d0a98428757e3d72d153709f87ad798414a3901ca0fffbabbba9724924f6`
- `retroarch/config/retroarch-practical.cfg`:
  `945fc8ea1763111fc888425656ac52f6632f0d166aceb70e37662dc6e3857226`
- `retroarch/config/retroarch-minimal.cfg`:
  `993c93b9cb027ab2a0a5172ac79d9ba5e3e98a83f169943f836b1f27ba069468`
- `bin/plumos-retroarch-launch`:
  `54f7088eb6a840161ed14b11b0f787ac905875fba8fcf17f70eadbf827be3794`
- `bin/plumos-joystickd`:
  `8325ffcc7583b23aa51aecb69940815e0c73cab2766a28d4defebd2d4c8bc12e`
- `config/frontend/systems.json`:
  `464a8839cace825b459e7dec1a2f873f2aba83bb5cb7208ea12fa2a2a7c1d7ca`
- `state/frontend/core-overrides.json`:
  `07a44e925b368e24b51ea8a12dde9d373765fcb9c490f13ceae4b0217c8a34ce`

Representative validated cores:

- `fceumm_libretro.so`:
  `78cf0252a84ecc134855dc4949dc230a3acbc98ac71f0da30fbf85732370bb2e`
- `gambatte_libretro.so`:
  `c8d7191b22bb8d0e2d2a8f55f894f4b1765ea7b4014dbcfcd772088edc359484`
- `quicknes_libretro.so`:
  `8ea4cd5764654d65ea48a37014cd1f260a19969644999862508abb4b2fc5008e`

## Captured Practical Config Highlights

Important `retroarch-practical.cfg` values:

- `video_driver = "gl"`
- `video_context_driver = "mali_fbdev"`
- `video_rotation = "1"`
- `menu_driver = "rgui"`
- `audio_driver = "alsa"`
- `audio_device = "default"`
- `audio_latency = "64"`
- `input_driver = "null"`
- `input_joypad_driver = "sdl2"`
- `network_cmd_enable = "true"`
- `network_cmd_port = "55355"`
- `savestate_auto_load = "false"`
- `savestate_auto_save = "false"`

## Recovery Rule

When investigating RetroArch issues, compare `retroarch/` together with
`plumos-retroarch-launch`, `plumos-joystickd`, `systems.json`, and
`core-overrides.json`. Restoring only one core `.so`, config file, launch
profile, or override file can produce a state that no longer matches the working
conditions.

This backup is not the completed distribution baseline. A newer baseline may be
created after broader core, CD-system, arcade, save/state, resume, and per-ROM
profile validation.
