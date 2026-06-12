# Red Viper A30 設定

この文書は Virtual Boy 用 standalone `red-viper-a30` の設定項目を整理します。
ユーザーが変更できることを原則にし、A30 で即時反映できる項目、次回起動から反映する項目、
upstream にはあるが A30 backend が未実装の項目を分けて扱います。

## A30 in-game menu

`red-viper-a30` 実行中に Function (`KEY_ESC`) を押すと、A30 用の軽量メニューを表示します。
上下で移動、左右または A で変更、B または Function で戻ります。`START+SELECT` 終了は維持します。

| 項目 | 保存先 | 反映 | 内容 |
| --- | --- | --- | --- |
| Eye | `PLUMOS_A30_RED_VIPER_EYE` | 即時 | `both`, `left`, `right`。単眼表示で画面二重表示を避けられます。 |
| Scale | `PLUMOS_A30_RED_VIPER_SCALE` | 即時 | `fit`, `stretch`, `integer`。 |
| Rotation | `PLUMOS_A30_RED_VIPER_ROTATION` | 即時 | `ccw`, `cw`, `none`。 |
| Wait Vsync | `PLUMOS_A30_RED_VIPER_WAIT_VSYNC` | 即時 | framebuffer pan の vsync wait。ちらつき確認用に切替可能です。 |
| Color | `PLUMOS_A30_RED_VIPER_COLOR` | 即時 | `red`, `white`, `green`, `amber`, `blue`, または `R,G,B`。 |
| Frame Skip | `PLUMOS_A30_RED_VIPER_FRAME_SKIP` | 即時 | 表示 frame skip `0..3`。 |
| Fast Fwd | `PLUMOS_A30_RED_VIPER_FAST_FORWARD` | 即時 | frame pacing を外す fast-forward。 |
| FF Mode | `PLUMOS_A30_RED_VIPER_FF_TOGGLE` | 保存 | upstream の hold/toggle 設定。A30 では fast-forward 操作の拡張時に使います。 |
| VIP OC | `PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK` | 即時 | VIP 処理時間を短縮する upstream timing shortcut。 |
| Sound | `PLUMOS_A30_RED_VIPER_AUDIO` | 即時 | `alsa` / `off`。起動時に audio off だった場合の再有効化は backend 状態に依存します。 |
| Audio Lat | `PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US` | 次回起動 | ALSA latency。 |
| Prebuffer | `PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS` | 次回起動 | 起動時 audio prebuffer chunk 数。 |
| Queue | `PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS` | 次回起動 | producer queue chunk 数。 |
| CPU Policy | `PLUMOS_A30_RED_VIPER_CPU_POLICY` | 次回起動 | `fixed` / `performance`。launcher が適用し終了時に戻します。 |
| CPU Freq | `PLUMOS_A30_RED_VIPER_CPU_FREQ` | 次回起動 | `648000`, `816000`, `1008000`, `1200000`, `1344000`。 |
| CPU Cores | `PLUMOS_A30_RED_VIPER_CPU_CORES` | 次回起動 | `2` / `4`。launcher 方針に合わせます。 |
| Renderer | なし | 表示のみ | 現在の A30 backend は CPU renderer 固定です。 |
| Reset Game | なし | 即時 | emulation reset。 |
| Quit | なし | 即時 | emulator 終了。 |

## A30 launcher 設定

`/mnt/SDCARD/plumos/config/standalone/red_viper.env` は user-mutable とし、deploy では保持します。
in-game menu で変更した値もこの file に保存します。

A30 固有の設定は以下です。

- Display/input: `PLUMOS_A30_RED_VIPER_FB`, `PLUMOS_A30_RED_VIPER_INPUT`,
  `PLUMOS_A30_RED_VIPER_ROTATION`, `PLUMOS_A30_RED_VIPER_SCALE`,
  `PLUMOS_A30_RED_VIPER_EYE`, `PLUMOS_A30_RED_VIPER_COLOR`,
  `PLUMOS_A30_RED_VIPER_WAIT_VSYNC`
- Audio: `PLUMOS_A30_RED_VIPER_AUDIO`, `PLUMOS_A30_RED_VIPER_ALSA_DEVICE`,
  `PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US`,
  `PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS`,
  `PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS`
- Emulation/performance: `PLUMOS_A30_RED_VIPER_FRAME_SKIP`,
  `PLUMOS_A30_RED_VIPER_FAST_FORWARD`, `PLUMOS_A30_RED_VIPER_FF_TOGGLE`,
  `PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK`
- CPU launcher policy: `PLUMOS_A30_RED_VIPER_CPU_POLICY`,
  `PLUMOS_A30_RED_VIPER_CPU_FREQ`, `PLUMOS_A30_RED_VIPER_CPU_CORES`

## upstream Red Viper 設定 inventory

Red Viper upstream の `VB_OPT` には以下の設定が存在します。A30 では、移植 backend があるものから
順に menu/env へ接続します。

| 分類 | 主な upstream 項目 | A30 での扱い |
| --- | --- | --- |
| Video/stereo | `DSPMODE`, `DSPSWAP`, `DSP2X`, `SLIDERMODE`, `DEFAULT_EYE`, `ANAGLYPH`, `ANAGLYPH_LEFT`, `ANAGLYPH_RIGHT`, `ANAGLYPH_DEPTH` | A30 は単一 framebuffer なので、まず `Eye` と `Color` として接続。anaglyph/depth は A30 renderer 側の追加実装が必要です。 |
| Color/palette | `MULTICOL`, `TINT`, `MULTIID`, `MTINT`, `STINT`, `PALMODE`, `FIXPAL`, `BFACTOR` | A30 menu は単色 tint と任意 RGB を先に接続。multicolor palette editor は追加対象です。 |
| Render backend | `RENDERMODE`, `SOFT_FLUSH`, `DOUBLE_BUFFER`, `VSYNC` | A30 は 3DS GPU/Citro3D backend を持たないため `RM_CPUONLY` 固定。framebuffer 側 vsync は `Wait Vsync` として接続。 |
| Performance | `MAXCYCLES`, `FRMSKIP`, `FASTFORWARD`, `FF_TOGGLE`, `N3DS_SPEEDUP`, `VIP_OVERCLOCK`, `VIP_OVER_SOFT`, `ANTIFLICKER`, `PERF_INFO` | `FRMSKIP`, fast-forward, `VIP_OVERCLOCK` を接続済み。`N3DS_SPEEDUP` や `ANTIFLICKER` は 3DS path 依存のため A30 効果を確認してから接続します。 |
| Audio | `SOUND` | A30 ALSA backend では Sound/latency/prebuffer/queue を接続済み。 |
| Input | `ABXY_MODE`, `ZLZR_MODE`, `DPAD_MODE`, `CUSTOM_CONTROLS`, `CUSTOM_MAPPING_*`, `CUSTOM_MOD`, `INPUTS` | A30 物理 button mapping は wrapper 側で固定実装。ユーザー remap UI は追加対象です。 |
| Touch/3DS device | `TOUCH_*`, `TOUCH_SWITCH`, `CPP_ENABLED`, `PAUSE_RIGHT` | 3DS touch screen / Circle Pad Pro 用。A30 には同等 device がないため、代替 UI を作る場合だけ意味を持ちます。 |
| Multiplayer | `INPUT_BUFFER` と multiplayer path | 3DS local multiplayer 用。A30 backend は未実装です。 |
| Files/game | `ROM_PATH`, `RAM_PATH`, `LAST_ROM`, `HOME_PATH`, `GAME_SETTINGS`, `CRC32`, `GAME_ID` | A30 launcher が ROM/save path を設定。game-specific settings は今後 per-ROM override と接続します。 |

## 方針

- A30 で効果がある設定は menu/env へ接続します。
- 起動時にしか安全に変えられない設定は、保存して次回起動から反映します。
- 3DS 専用の設定は silently hide せず、A30 では backend 未実装として inventory に残します。
- `RM_GPUONLY`, `RM_TOGPU`, `RM_TOCPU` は Citro3D/GPU backend 前提のため、A30 では現時点でユーザー変更しても実行可能な backend がありません。
