# ScummVM 現時点動作バックアップ基準

この文書は、2026-06-11 時点で正しく動作している ScummVM standalone 環境を、
今後の復旧基準として固定するための記録です。

## バックアップ

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

## 状態の位置付け

このバックアップは、ScummVM standalone の現時点動作基準です。A30 向けの画面回転、
VirtualMouse warp 修正、`scummmodern-a30-md` theme、音声、入力、終了動線が
first-pass 済みの状態として扱います。

一方で、ScummVM の全ゲーム/全 engine を検証済みという意味ではありません。今後
ScummVM に表示、マウス、音、theme、保存、終了動線の問題が出た場合は、この
バックアップを「戻れる状態」として比較してください。

## バックアップ範囲

- `/mnt/SDCARD/plumos/emulators/scummvm/`
  - ScummVM binary、A30 patch 済み binary backup、theme、engine data。
- `/mnt/SDCARD/plumos/state/standalone/scummvm/`
  - ScummVM config、log、save data、過去調整時の config backup。
- `/mnt/SDCARD/plumos/bin/plumos-standalone-launch`
  - ScummVM 起動時の SDL、CPU、target 解決、config 注入、theme path を決める launcher。
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - ScummVM standalone launch 時の A30 virtual gamepad path。
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - FE から選択される ScummVM launch profile の元データ。
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - system/ROM 別 override 状態。

`/mnt/SDCARD/plumos/config/standalone/scummvm.env` と
`/mnt/SDCARD/plumos/config/standalone/standalone.env` は、この時点では存在しません。
ScummVM の現在の起動条件は `plumos-standalone-launch` の ScummVM branch 既定値と、
`state/standalone/scummvm` の config で決まります。

## 主要 hash

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

## Captured config highlights

`state/standalone/scummvm/config/scummvm.ini` の主要値:

- `rotation_mode=270`
- `gui_theme=scummmodern-a30-md`
- `gfx_mode=surfacesdl`
- `fullscreen=true`
- `aspect_ratio=true`
- `stretch_mode=fit`
- `filtering=false`
- `vsync=true`

## 復旧ルール

ScummVM の不具合を調査する場合、`emulators/scummvm/` だけでなく、
`state/standalone/scummvm/`、`plumos-standalone-launch`、`plumos-joystickd`、
`systems.json`、`core-overrides.json` を同じ組として比較します。ScummVM の動作は
binary/theme だけでなく、launcher の target 解決、A30 向け SDL/CPU 既定値、
ScummVM config、FE launch profile の組み合わせで決まります。

このバックアップは現時点の復旧基準であり、将来 ScummVM の対応 game/engine、save、
state、per-game profile の検証が進んだ場合は、別の baseline を作る可能性があります。
