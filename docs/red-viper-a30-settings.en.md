# Red Viper A30 Settings

This document inventories settings for the Virtual Boy standalone
`red-viper-a30`. The policy is to make settings user-changeable where the A30
backend can apply them, while clearly separating immediate settings,
next-launch settings, and upstream settings that still need an A30 backend.

As of 2026-06-13, the standard Virtual Boy profile is the optimized
`retroarch:mednafen_vb` core. Red Viper standalone builds the StockOS-derived
SDL2 `mali` + GLES2 rendering path as `red-viper-sdlgl-a30`. Orientation and
fit, single-eye rendering, and SDL audio queue settings are wired, but Bad
Apple still drops audio in heavy scenes. Red Viper is therefore removed from
normal distribution and the FE profile list. Build it explicitly with
`PLUMOS_STANDALONE_FILTER=red_viper` only when probing.

## A30 In-Game Menu

Press Function (`KEY_ESC`) while `red-viper-a30` is running to open the A30 menu.
Up/down moves, left/right or A changes a value, and B or Function returns.
`START+SELECT` still exits.

| Item | Stored as | Applies | Notes |
| --- | --- | --- | --- |
| Eye | `PLUMOS_A30_RED_VIPER_EYE` | Immediate | `both`, `left`, `right`. Useful for avoiding double-image output on the single A30 screen. |
| Scale | `PLUMOS_A30_RED_VIPER_SCALE` | Immediate | `fit`, `stretch`, `integer`. |
| Rotation | `PLUMOS_A30_RED_VIPER_ROTATION` | Immediate | `ccw`, `cw`, `none`. |
| Wait Vsync | `PLUMOS_A30_RED_VIPER_WAIT_VSYNC` | Immediate | Framebuffer pan vsync wait. Useful when checking flicker. |
| Color | `PLUMOS_A30_RED_VIPER_COLOR` | Immediate | `red`, `white`, `green`, `amber`, `blue`, or `R,G,B`. |
| Frame Skip | `PLUMOS_A30_RED_VIPER_FRAME_SKIP` | Immediate | Skips A30 framebuffer presentation `0..3`; Red Viper's internal software render still runs every frame, and this does not enable fast-forward. |
| Fast Fwd | `PLUMOS_A30_RED_VIPER_FAST_FORWARD` | Immediate | Removes frame pacing. |
| FF Mode | `PLUMOS_A30_RED_VIPER_FF_TOGGLE` | Stored | Upstream hold/toggle setting, reserved for A30 fast-forward hotkey expansion. |
| VIP OC | `PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK` | Immediate | Upstream VIP timing shortcut. |
| Perf Info | `PLUMOS_A30_RED_VIPER_PERF_INFO` | Immediate | Shows `EMU`, `SPD`, `PRES`, `REND`, `LATE`, and `AUD` in a top-left overlay. |
| Sound | `PLUMOS_A30_RED_VIPER_AUDIO` | Immediate | `alsa` / `off`. Re-enabling from an audio-off launch depends on backend state. |
| Audio Lat | `PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US` | Next launch | ALSA latency. |
| Prebuffer | `PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS` | Next launch | Startup audio prebuffer chunks. |
| Queue | `PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS` | Next launch | Producer queue chunks. |
| Audio Gap | `PLUMOS_A30_RED_VIPER_AUDIO_GAP` | Immediate | Gap fill used when audio generation cannot keep up: `fade`, `silence`, `repeat`. |
| CPU Policy | `PLUMOS_A30_RED_VIPER_CPU_POLICY` | Next launch | `fixed` / `performance`; the launcher applies and restores it. |
| CPU Freq | `PLUMOS_A30_RED_VIPER_CPU_FREQ` | Next launch | `648000`, `816000`, `1008000`, `1200000`, `1344000`. |
| CPU Cores | `PLUMOS_A30_RED_VIPER_CPU_CORES` | Next launch | `2` / `4`. |
| Renderer | `PLUMOS_A30_RED_VIPER_RENDERER` | Next launch | `sdlgl` / `fbdev`. The default is the stock SDL2 `mali` + GLES2 `sdlgl` path. fbdev is the older wrapper. |
| Reset Game | none | Immediate | Emulation reset. |
| Quit | none | Immediate | Exit emulator. |

## A30 Launcher Settings

`/mnt/SDCARD/plumos/config/standalone/red_viper.env` is user-mutable and
preserved by deploy. Values changed in the in-game menu are written back there.

A30-specific settings are:

- Renderer/display/input: `PLUMOS_A30_RED_VIPER_RENDERER`,
  `PLUMOS_A30_RED_VIPER_FB`, `PLUMOS_A30_RED_VIPER_INPUT`,
  `PLUMOS_A30_RED_VIPER_ROTATION`, `PLUMOS_A30_RED_VIPER_SCALE`,
  `PLUMOS_A30_RED_VIPER_SDLGL_ROTATION`, `PLUMOS_A30_RED_VIPER_SDLGL_SCALE`,
  `PLUMOS_A30_RED_VIPER_SDLGL_PHYSICAL`,
  `PLUMOS_A30_RED_VIPER_EYE`, `PLUMOS_A30_RED_VIPER_COLOR`,
  `PLUMOS_A30_RED_VIPER_WAIT_VSYNC`
- Audio: `PLUMOS_A30_RED_VIPER_AUDIO`, `PLUMOS_A30_RED_VIPER_ALSA_DEVICE`,
  `PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US`,
  `PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS`,
  `PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS`,
  `PLUMOS_A30_RED_VIPER_AUDIO_GAP`
- Emulation/performance: `PLUMOS_A30_RED_VIPER_FRAME_SKIP`,
  `PLUMOS_A30_RED_VIPER_FAST_FORWARD`, `PLUMOS_A30_RED_VIPER_FF_TOGGLE`,
  `PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK`,
  `PLUMOS_A30_RED_VIPER_PERF_INFO`
- CPU launcher policy: `PLUMOS_A30_RED_VIPER_CPU_POLICY`,
  `PLUMOS_A30_RED_VIPER_CPU_FREQ`, `PLUMOS_A30_RED_VIPER_CPU_CORES`

## Upstream Red Viper Inventory

Upstream Red Viper's `VB_OPT` contains the following settings. The A30 port
connects the settings that have a meaningful A30 backend first.

| Category | Upstream fields | A30 handling |
| --- | --- | --- |
| Video/stereo | `DSPMODE`, `DSPSWAP`, `DSP2X`, `SLIDERMODE`, `DEFAULT_EYE`, `ANAGLYPH`, `ANAGLYPH_LEFT`, `ANAGLYPH_RIGHT`, `ANAGLYPH_DEPTH` | The A30 has one framebuffer, so `Eye` and `Color` are connected first. Anaglyph/depth need extra A30 renderer work. |
| Color/palette | `MULTICOL`, `TINT`, `MULTIID`, `MTINT`, `STINT`, `PALMODE`, `FIXPAL`, `BFACTOR` | The menu currently exposes mono tint and arbitrary RGB. A multicolor palette editor is future work. |
| Render backend | `RENDERMODE`, `SOFT_FLUSH`, `DOUBLE_BUFFER`, `VSYNC` | The fbdev A30 backend runs `RM_CPUONLY`. The stock SDL2 `mali` + GLES2 `sdlgl` backend uses `Mali-400 MP` and rotates the final quad into the A30 `480x640` raw framebuffer as a landscape image. The 2026-06-13 Bad Apple 120 second run still averaged 35.18fps and bottomed at 14.49fps, so it is not the standard profile for performance reasons. Framebuffer vsync is exposed through the fbdev wrapper's `Wait Vsync`. |
| Performance | `MAXCYCLES`, `FRMSKIP`, `FASTFORWARD`, `FF_TOGGLE`, `N3DS_SPEEDUP`, `VIP_OVERCLOCK`, `VIP_OVER_SOFT`, `ANTIFLICKER`, `PERF_INFO` | `FRMSKIP`, fast-forward, and `VIP_OVERCLOCK` are connected. `N3DS_SPEEDUP` and `ANTIFLICKER` need A30 effect checks. |
| Audio | `SOUND` | The A30 ALSA backend exposes sound, latency, prebuffer, queue, and gap-fill settings. |
| Input | `ABXY_MODE`, `ZLZR_MODE`, `DPAD_MODE`, `CUSTOM_CONTROLS`, `CUSTOM_MAPPING_*`, `CUSTOM_MOD`, `INPUTS` | A30 physical mapping is currently implemented in the wrapper. User remap UI is future work. |
| Touch/3DS device | `TOUCH_*`, `TOUCH_SWITCH`, `CPP_ENABLED`, `PAUSE_RIGHT` | These are for 3DS touchscreen / Circle Pad Pro. They only matter on A30 if an equivalent UI is implemented. |
| Multiplayer | `INPUT_BUFFER` and multiplayer path | 3DS local multiplayer path. A30 backend is not implemented. |
| Files/game | `ROM_PATH`, `RAM_PATH`, `LAST_ROM`, `HOME_PATH`, `GAME_SETTINGS`, `CRC32`, `GAME_ID` | The A30 launcher sets ROM/save paths. Game-specific settings should connect to per-ROM overrides later. |

## Policy

- Settings that affect the A30 backend are exposed through menu/env.
- Settings that are only safe at launch are saved and applied on the next run.
- 3DS-only settings are not silently hidden; they remain inventoried as backend gaps.
- `RM_GPUONLY`, `RM_TOGPU`, and `RM_TOCPU` assume the Citro3D/GPU backend. A stock SDL2 `mali` + GLES2 frontend experiment confirmed a partial A30 benefit, and orientation/fit plus single-eye rendering are now handled in the GLES path. Heavy scenes still drop audio, and a deeper SDL audio queue did not materially improve it, so Red Viper is removed from normal distribution and the FE profile list.

## Reading Perf Info

- `EMU` / `SPD`: emulation frame rate and speed ratio. Around 50.0fps / `100%` means full Virtual Boy speed. Below 50 means slowdown; above 50 means fast-forward.
- `PRES`: fps actually presented to the A30 framebuffer. This should drop when Frame Skip is raised.
- `REND`: Red Viper software-render fps. It should usually stay near `EMU`.
- `LATE`: percentage of frames where the 50Hz pacing deadline was missed and no sleep occurred.
- `AUD R/D/X`: audio repeat/drop/xrun counts over roughly the last second. If `R` rises, audio generation is not keeping up with real time and gap fill is active.
