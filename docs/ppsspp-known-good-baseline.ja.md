# PPSSPP known-good recovery baseline

この文書は、2026-06-11 にユーザーが「今の状態があらゆる面で正しい」と確認した
PPSSPP standalone 環境を、今後の復旧基準として固定するための記録です。

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

The backup must be treated as a set. If PPSSPP breaks again, do not restore only
one obvious file first; compare these pieces together:

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
  - The launcher branch that exports PPSSPP display/input/CPU variables.
  - Captured SHA256:
    `db3e1e591bda188c6cadb5e06901e79055baa0cdc1afd22f5630abf0ead0f1ca`
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - The A30 virtual gamepad path used by standalone emulators.
  - Captured SHA256:
    `8325ffcc7583b23aa51aecb69940815e0c73cab2766a28d4defebd2d4c8bc12e`

## Key launcher environment

The captured `ppsspp.env` has the following important behavior:

- Use stock SDL path: `PLUMOS_STANDALONE_USE_STOCK_SDL=1`
- The stock SDL library search path must include both
  `/mnt/SDCARD/miyoo/lib` and `/usr/miyoo/lib`. Fresh SD-root packages do not
  necessarily contain `/mnt/SDCARD/miyoo/lib`, but the A30 rootfs provides the
  stock SDL2 `mali` backend under `/usr/miyoo/lib`.
- CPU policy: `fixed`, `1344000`, `4` cores
- Display: `PLUMOS_A30_DISPLAY_ROTATION=ccw`
- Logical UI size: `PLUMOS_A30_DISPLAY_LOGICAL=854x480`
- Force landscape: `PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=1`
- Pause mapping exported to launcher: `PLUMOS_A30_PSP_PAUSE_MAPPING=10-104`
- joystickd trigger mode: `buttons`
- joystickd shoulder layout: `standard`
- joystickd axis sources: `axisYR` and `axisXR`
- PPSSPP append profile: disabled
- Escape-to-exit: disabled
- Automatic PPSSPP controls repair: disabled with
  `PLUMOS_A30_PSP_CONTROLS=none` and `PLUMOS_A30_PSP_FORCE_CONTROLS=0`

## Captured PPSSPP settings

Active config:

- `config/ppsspp/PSP/SYSTEM/ppsspp.ini`
  - `UIScaleFactor = -2`
  - `AutoFrameSkip = True`
  - `AudioBufferSize = 512`
  - Landscape `DisplayAspectRatio = 0.562500`
  - Landscape `InternalScreenRotation = 1`
  - Landscape `RotateControlsWithScreen = False`
- `config/ppsspp/PSP/SYSTEM/controls.ini`
  - `Pause = 10-193`

Legacy `.config` mirror:

- `.config/ppsspp/PSP/SYSTEM/ppsspp.ini`
  - `UIScaleFactor = -2`
  - `AutoFrameSkip = False`
  - `AudioBufferSize = 256`
  - Landscape `DisplayAspectRatio = 0.562500`
  - Landscape `InternalScreenRotation = 1`
  - Landscape `RotateControlsWithScreen = True`
- `.config/ppsspp/PSP/SYSTEM/controls.ini`
  - `Pause = 10-104`

The active and legacy files are intentionally recorded as they existed on the
A30. Do not normalize them during recovery unless the user explicitly asks for
that specific operation.

## 横メニューが成立する理由

stock/plumOS の PPSSPP 横メニューは、PPSSPP 側の単独設定だけで成立しているわけでは
ありません。A30 固有の platform contract として、以下の要素が揃っている必要があります。

- `PLUMOS_STANDALONE_USE_STOCK_SDL=1` により、PPSSPP は StockOS SDL2 経路を使う。
  この経路では `/usr/miyoo/lib` の A30 `mali` backend が使われ、`PPSSPPSDL` が
  SDL window を作成してから GLES renderer を使う前提と噛み合う。
- launcher が A30 向けの論理横画面を export する:
  `PLUMOS_A30_DISPLAY_ROTATION=ccw`,
  `PLUMOS_A30_DISPLAY_LOGICAL=854x480`,
  `PLUMOS_A30_DISPLAY_FORCE_LANDSCAPE=1`。物理 framebuffer は A30 の縦向き panel の
  ままですが、PPSSPP の UI/layout 側には PSP 的な横画面 geometry を見せる。
- PPSSPP の user config 側で `DisplayAspectRatio = 0.562500`,
  `InternalScreenRotation = 1`, `RotateControlsWithScreen = False`,
  `UIScaleFactor = -2` を使い、この論理横画面 contract を完成させる。
- 入力も同じ contract の一部。StockOS は `miyoo282_xpad_inputd` を起動し、
  plumOS は同じ考え方を `plumos-joystickd --device-mode xbox` で再現する。これにより
  PPSSPP は A30 の raw serial input ではなく、通常の SDL2 GameController 風の
  virtual pad を読む。

official/vanilla PPSSPP をこれらの launcher 変数と config default なしで起動すると、
SDL display を A30 の縦向き画面として解釈しやすく、menu layout/aspect が崩れたり、
controls が合わない状態になりやすい。動作する横メニューは、StockOS SDL2/Mali runtime
選択、launcher の display 変数、PPSSPP config、virtual gamepad setup の組み合わせとして
扱う。

## Recovery rule

This backup is the first recovery anchor for future PPSSPP issues. Display,
menu aspect, controller behavior, L2/menu behavior, launcher variables,
PPSSPP binary/assets, and user-managed config should be compared against this
snapshot before changing code or writing new defaults.

The launcher must continue to treat PPSSPP config and controls as user-managed
state. Normal launch and deploy flows must not auto-reset or auto-repair
`ppsspp.ini` or `controls.ini`. Explicit repair modes are allowed only when the
user asks for them.

## Fresh SD-root rule

Fresh SD-root packages must include a sanitized PPSSPP factory state under
`plumos/state/standalone/ppsspp/`, containing only:

- `config/plumos-a30-ppsspp-layout.ini`
- `config/ppsspp/PSP/SYSTEM/ppsspp.ini`
- `config/ppsspp/PSP/SYSTEM/controls.ini`
- `.config/ppsspp/PSP/SYSTEM/ppsspp.ini`
- `.config/ppsspp/PSP/SYSTEM/controls.ini`

Do not include PPSSPP `SAVEDATA`, shader cache, temporary `.before-*` files, or
top-level `/mnt/SDCARD/.config/ppsspp` in release packages. The factory state is
the seed for a clean SD card; existing user-managed PPSSPP state should still be
preserved by normal runtime deploy/install flows.
