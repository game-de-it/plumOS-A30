# ScummVM Current Working Baseline

This document fixes the ScummVM standalone environment that was working on
2026-06-11. Use it as the recovery baseline for future ScummVM display, input,
theme, audio, save, or exit regressions.

Japanese counterpart: [scummvm-known-working-baseline.ja.md](scummvm-known-working-baseline.ja.md)

## Backup

- Backup ID: `scummvm-known-working-20260611-071133`
- A30 backup: `/mnt/SDCARD/plumos/backups/scummvm-known-working-20260611-071133`
- Local artifact:
  `artifacts/a30-backups/scummvm-known-working-20260611-071133/scummvm-known-working-20260611-071133.tar`
- Local artifact SHA256:
  `9c011411205739c7918b3a7446c24506ca7f1ee57ccc48c22be82299260bdf98`
- Manifest:
  `artifacts/a30-backups/scummvm-known-working-20260611-071133/MANIFEST.txt`

ROM/game data directories are not part of this backup. The snapshot covers the
plumOS ScummVM runtime, launcher inputs, and ScummVM-owned state needed to
reproduce the current working state.

## Status

This is the current ScummVM standalone working baseline. Treat it as the
first-pass validated state for A30 display rotation, VirtualMouse warp fixes,
`scummmodern-a30-md` theme, audio, input, and exit flow.

It does not mean every ScummVM game or engine has been validated.

## Backup Scope

- `/mnt/SDCARD/plumos/emulators/scummvm/`
  - ScummVM binary, A30-patched binary backup, theme, and engine data.
- `/mnt/SDCARD/plumos/state/standalone/scummvm/`
  - ScummVM config, logs, save data, and historical config backups.
- `/mnt/SDCARD/plumos/bin/plumos-standalone-launch`
  - Launcher that resolves SDL, CPU, target, config injection, and theme path.
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - A30 virtual gamepad path used by standalone ScummVM.
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - Source data for FE-selected ScummVM launch profiles.
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - System/ROM override state.

At this snapshot time, `/mnt/SDCARD/plumos/config/standalone/scummvm.env` and
`/mnt/SDCARD/plumos/config/standalone/standalone.env` did not exist. Current
ScummVM launch behavior was determined by the ScummVM branch defaults in
`plumos-standalone-launch` plus `state/standalone/scummvm` config.

## Key Hashes

- `emulators/scummvm/bin/scummvm`:
  `cdf718559c8b5aad8fd158219b34780b7ce06939d0519a6732f6f4c4a4be8cd1`
- `state/standalone/scummvm/config/scummvm.ini`:
  `9514776937c3e97535a7fb8297bfbf61c6c3449f31cc0a897e9afd5a41f81245`
- `state/standalone/scummvm/.config/scummvm/scummvm.ini`:
  `4b5fd99b16241d9efcd06ea89097833e7a9481c1b4b6115b6a2c241b77b6af4c`
- `bin/plumos-standalone-launch`:
  `db3e1e591bda188c6cadb5e06901e79055baa0cdc1afd22f5630abf0ead0f1ca`
- `bin/plumos-joystickd`:
  `8325ffcc7583b23aa51aecb69940815e0c73cab2766a28d4defebd2d4c8bc12e`
- `config/frontend/systems.json`:
  `464a8839cace825b459e7dec1a2f873f2aba83bb5cb7208ea12fa2a2a7c1d7ca`
- `state/frontend/core-overrides.json`:
  `07a44e925b368e24b51ea8a12dde9d373765fcb9c490f13ceae4b0217c8a34ce`
- `emulators/scummvm/share/scummvm/scummmodern-a30-md.zip`:
  `83a7fddf26b2bbdf0c7b83ce4d1b896f6a6e2a5ceb28b7887718dabb69cb4bd0`

## Captured Config Highlights

Important `state/standalone/scummvm/config/scummvm.ini` values:

- `rotation_mode=270`
- `gui_theme=scummmodern-a30-md`
- `gfx_mode=surfacesdl`
- `fullscreen=true`
- `aspect_ratio=true`
- `stretch_mode=fit`
- `filtering=false`
- `vsync=true`

## Recovery Rule

When investigating ScummVM issues, compare `emulators/scummvm/` together with
`state/standalone/scummvm/`, `plumos-standalone-launch`, `plumos-joystickd`,
`systems.json`, and `core-overrides.json`. ScummVM behavior depends on the
binary/theme, launcher target resolution, A30 SDL/CPU defaults, ScummVM config,
and FE launch profile together.

This backup is the current recovery point. A newer baseline may be created after
more ScummVM game/engine, save, state, and per-game profile validation.
