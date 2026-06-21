# RetroArch 現時点動作バックアップ基準

この文書は、2026-06-11 時点で「完全版ではないが、現時点で正しく動作している」
RetroArch runtime を復旧基準として固定するための記録です。

## バックアップ

- Backup ID: `retroarch-known-working-20260611-070551`
- A30 backup: `/mnt/SDCARD/plumos/backups/retroarch-known-working-20260611-070551`
- Local artifact:
  `artifacts/a30-backups/retroarch-known-working-20260611-070551/retroarch-known-working-20260611-070551.tar`
- Local artifact SHA256:
  `c718a3432c24dce3d1c4285de0ba7fd91a0929a7d94917e5f315e687c053761b`
- Manifest:
  `artifacts/a30-backups/retroarch-known-working-20260611-070551/MANIFEST.txt`

ROM files are not part of this backup. The snapshot covers the plumOS RetroArch
runtime and the launch/profile files needed to reproduce the current working
state.

## 状態の位置付け

このバックアップは PPSSPP の known-good baseline と違い、RetroArch 全体の完成版では
ありません。`fceumm`、`gambatte`、practical runtime の音声/入力経路、FE からの
RetroArch launch、safe exit 系の主要経路が現時点で正しく動いている状態として扱います。

一方で、41 core すべての system 別起動、性能、音声、入力、save/state 挙動はまだ
検証途中です。今後 RetroArch に障害が出た場合は、このバックアップを「現時点で戻れる
動作基準」として比較してください。

## バックアップ範囲

- `/mnt/SDCARD/plumos/retroarch/`
  - RetroArch binary、config、core、info、autoconfig、save/state/log 等。
  - Captured core count: `41`
- `/mnt/SDCARD/plumos/bin/plumos-retroarch-launch`
  - RetroArch 起動時の config、core、CPU、audio/input、safe exit を組み立てる launcher。
- `/mnt/SDCARD/plumos/bin/plumos-joystickd`
  - RetroArch SDL2 joypad 経路で使う A30 virtual gamepad。
- `/mnt/SDCARD/plumos/config/frontend/systems.json`
  - FE から選択される RetroArch launch profile の元データ。
- `/mnt/SDCARD/plumos/state/frontend/core-overrides.json`
  - system/ROM 別の core、CPU、audio 等の override 状態。

## 主要 hash

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

## Captured practical config highlights

`retroarch-practical.cfg` の主要値:

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

## 復旧ルール

RetroArch の不具合を調査する場合、`retroarch/` だけでなく、
`plumos-retroarch-launch`、`plumos-joystickd`、`systems.json`、
`core-overrides.json` を同じ組として比較します。core の `.so`、config、FE の
launch profile、system/ROM override のどれか一つだけを戻すと、現在の動作条件と
ずれる可能性があります。

このバックアップは完成版の配布基準ではありません。今後の全 core 検証、CD 系、
arcade 系、save/state/resume、per-ROM profile の検証が進んだ段階で、別の
baseline を作る可能性があります。
