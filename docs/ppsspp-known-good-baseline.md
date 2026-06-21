# PPSSPP Known-Good Recovery Baseline

This document fixes the PPSSPP standalone environment that the user confirmed on
2026-06-11 as correct in all important aspects. Use it as the recovery baseline
for future PPSSPP issues.

Japanese counterpart: [ppsspp-known-good-baseline.ja.md](ppsspp-known-good-baseline.ja.md)

## Backup

- Backup ID: `ppsspp-known-good-20260611-064446`
- A30 backup: `/mnt/SDCARD/plumos/backups/ppsspp-known-good-20260611-064446`
- Local artifact:
  `artifacts/a30-backups/ppsspp-known-good-20260611-064446/ppsspp-known-good-20260611-064446.tar`
- Local artifact SHA256:
  `1bc1d6b1f24d5555c9896eac85f812b009e09a8dd8dcbc50b6b5123da6859acf`
- Manifest:
  `artifacts/a30-backups/ppsspp-known-good-20260611-064446/MANIFEST.txt`

ROM files are not part of this backup. The snapshot covers only the plumOS
PPSSPP runtime needed for recovery.

## Scope

Treat the backup as a set. If PPSSPP breaks again, compare these pieces
together instead of restoring one obvious-looking file first:

- `/mnt/SDCARD/plumos/emulators/ppsspp/`
  - PPSSPP binary and assets.
  - Captured `PPSSPPSDL` SHA256:
    `994f44a15890a1b10d670212319b982a0a35df380ae7c78d0edd7e91b42f4e23`
- `/mnt/SDCARD/plumos/state/standalone/ppsspp/`
  - User-managed PPSSPP config, controls, cache, and runtime state.
- `/mnt/SDCARD/plumos/config/standalone/ppsspp.env`
  - Launcher environment overrides for the A30 PPSSPP profile.
  - Captured SHA256:
    `bf7078292438aca78a1869f5480d650614fbe0000f331ff242748c9e4f8896f7`
- `/mnt/SDCARD/plumos/bin/plumos-standalone-launch`
  - Launcher branch that exports PPSSPP display/input/CPU variables.
  - Captured SHA256:
    `db3e1e591bda188c6cadb5e06901e79055baa0cdc1afd22f5630abf0ead0f1ca`
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - A30 virtual gamepad path used by standalone emulators.
  - Captured SHA256:
    `8325ffcc7583b23aa51aecb69940815e0c73cab2766a28d4defebd2d4c8bc12e`

## Key Launcher Environment

The captured `ppsspp.env` has these important properties:

- stock SDL path enabled: `PLUMOS_STANDALONE_USE_STOCK_SDL=1`
- stock SDL library search path must include both `/mnt/SDCARD/miyoo/lib` and
  `/usr/miyoo/lib`; fresh SD-root packages may omit `/mnt/SDCARD/miyoo/lib`, but
  the A30 rootfs provides the stock SDL2 `mali` backend under `/usr/miyoo/lib`
- CPU policy: `fixed`, `1344000`, `4` cores
- display rotation: `PLUMOS_A30_DISPLAY_ROTATION=ccw`
- logical UI size: `PLUMOS_A30_DISPLAY_LOGICAL=854x480`
- force landscape: `PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=1`
- pause mapping: `PLUMOS_A30_PSP_PAUSE_MAPPING=10-104`
- joystickd trigger mode: `buttons`
- joystickd shoulder layout: `standard`
- joystickd axis sources: `axisYR` and `axisXR`
- PPSSPP append profile: disabled
- escape-to-exit: disabled
- automatic PPSSPP controls repair disabled with
  `PLUMOS_A30_PSP_CONTROLS=none` and `PLUMOS_A30_PSP_FORCE_CONTROLS=0`

## Captured PPSSPP Settings

Active config:

- `config/ppsspp/PSP/SYSTEM/ppsspp.ini`
  - `UIScaleFactor = -2`
  - `AutoFrameSkip = True`
  - `AudioBufferSize = 512`
  - landscape `DisplayAspectRatio = 0.562500`
  - landscape `InternalScreenRotation = 1`
  - landscape `RotateControlsWithScreen = False`
- `config/ppsspp/PSP/SYSTEM/controls.ini`
  - `Pause = 10-193`

Legacy `.config` mirror:

- `.config/ppsspp/PSP/SYSTEM/ppsspp.ini`
  - `UIScaleFactor = -2`
  - `AutoFrameSkip = False`
  - `AudioBufferSize = 256`
  - landscape `DisplayAspectRatio = 0.562500`
  - landscape `InternalScreenRotation = 1`
  - landscape `RotateControlsWithScreen = True`
- `.config/ppsspp/PSP/SYSTEM/controls.ini`
  - `Pause = 10-104`

The active and legacy files are recorded exactly as they existed on the A30. Do
not normalize them during recovery unless the user explicitly asks for that.

## Recovery Rule

This backup is the first recovery anchor for future PPSSPP issues. Display,
menu aspect, controller behavior, L2/menu behavior, launcher variables, PPSSPP
binary/assets, and user-managed config should be compared against this snapshot
before changing code or writing new defaults.

The launcher must continue to treat PPSSPP config and controls as user-managed
state. Normal launch and deploy flows must not auto-reset or auto-repair
`ppsspp.ini` or `controls.ini`. Explicit repair modes are allowed only when the
user asks for them.

## Fresh SD-Root Rule

Fresh SD-root packages must include sanitized PPSSPP factory state under
`plumos/state/standalone/ppsspp/`, containing only:

- `config/plumos-a30-ppsspp-layout.ini`
- `config/ppsspp/PSP/SYSTEM/ppsspp.ini`
- `config/ppsspp/PSP/SYSTEM/controls.ini`
- `.config/ppsspp/PSP/SYSTEM/ppsspp.ini`
- `.config/ppsspp/PSP/SYSTEM/controls.ini`

Do not include PPSSPP `SAVEDATA`, shader cache, temporary `.before-*` files, or
top-level `/mnt/SDCARD/.config/ppsspp` in release packages. The factory state is
the seed for a clean SD card; existing user-managed PPSSPP state is still
preserved by normal runtime deploy/install flows.
