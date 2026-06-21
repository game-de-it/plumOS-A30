# FE Executable Targets

This inventory cross-checks cores built for plumOS, runtimes deployed to the
A30, and launch profiles selectable from the frontend. Only rows with
`target_for_verification=yes` are active verification targets.

Japanese counterpart: [emulator-fe-targets.ja.md](emulator-fe-targets.ja.md)

## Output Files

- `docs/emulator-fe-libretro-targets.tsv`
  - FE-executable RetroArch (`RA`) and PicoArch (`PICO`) profiles.
  - Includes profiles explicitly written as `picoarch:*` and companion profiles
    that the FE can derive from RetroArch profiles.
- `docs/emulator-fe-standalone-targets.tsv`
  - FE-executable `standalone:*` profiles and the non-libretro `pyxel:a30`
    profile.

## Rule Definitions

- `built_core=yes`: the core exists as a plumOS-built core in
  `docs/libretro-built-cores.tsv`.
- `deployed_core=yes`: the core exists on device under
  `/mnt/SDCARD/plumos/retroarch/cores/*_libretro.so`.
- `fe_selectable=yes`: the profile belongs to an enabled system or is a
  frontend-generated PicoArch companion.
- `fe_executable=yes`: the runtime/core/launcher needed to build a launch plan
  exists on the device.
- `target_for_verification=yes`: build, deployment, and FE execution conditions
  are all satisfied, and `verification_status` is not `retired`.
- `verification_status`: current result from
  `docs/emulator-runtime-verification.tsv`.
  - `pass`: video, audio, input, and performance are practical.
  - `pass_init`: boot or initial display is fixed, but gameplay/input/audio or
    performance needs more confirmation.
  - `fail_audio`: video/boot works but audio or codec support is not practical.
  - `fail_boot`: FE can attempt boot, but the game/content cannot start.
  - `fail_input`: display and execution work, but input mapping is not
    practical.
  - `fail_perf`: boot works, but performance, audio underrun, or frame pacing is
    not practical.
  - `fail_video`: boot works, but display corruption prevents practical use.
  - `fail`: no usable display, no boot, or another blocker prevents validation.
  - `retired`: removed from normal FE/verification targets by policy.
  - `untested`: FE-executable but not yet tested on hardware.

## PicoArch Companion Rules

PicoArch profiles can be written explicitly in `systems.json` as
`picoarch:*`. When no explicit profile exists, the FE may add a PicoArch
companion from a matching `retroarch:*` profile.

Known-problem or better-routed cores are excluded from automatic PicoArch
companions:

```text
easyrpg, freeintv, mame2003_plus, mednafen_pce, nekop2, nxengine,
np2kai, quasi88, retro8, squirreljme, tgbdual
```

For the `arcade` system, PicoArch companions for `fbneo`, `fbalpha2012`, and
`mame2000` are also excluded from normal candidates. Some of these cores are
confirmed as PICO-pass under other systems, so the exclusion is system+core
specific rather than a global core ban.

N64 was a Class C experiment. It is excluded from formal verification because of
performance and display-route constraints. StockOS-derived N64 cores are not
part of the plumOS build/deploy inventory.

## Regeneration

Collect deployed state from the device, then regenerate the TSV files:

```sh
mkdir -p artifacts/a30-core-inventory

./scripts/run-a30.sh 'for f in /mnt/SDCARD/plumos/retroarch/cores/*_libretro.so; do
  [ -f "$f" ] || continue
  b=${f##*/}
  b=${b%_libretro.so}
  echo "$b"
done | sort' > artifacts/a30-core-inventory/deployed-libretro-cores-20260618.txt

./scripts/run-a30.sh 'for p in \
  /mnt/SDCARD/plumos/bin/plumos-picoarch-launch \
  /mnt/SDCARD/plumos/bin/plumos-standalone-launch \
  /mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch \
  /mnt/SDCARD/plumos/emulators/pcsx_rearmed/bin/pcsx \
  /mnt/SDCARD/plumos/emulators/ppsspp/bin/PPSSPPSDL \
  /mnt/SDCARD/plumos/emulators/easyrpg/bin/easyrpg-player \
  /mnt/SDCARD/plumos/emulators/openbor/bin/OpenBOR \
  /mnt/SDCARD/plumos/emulators/scummvm/bin/scummvm \
  /mnt/SDCARD/plumos/emulators/dosbox-staging/bin/dosbox \
  /mnt/SDCARD/plumos/emulators/ppsspp-display-ui/bin/PPSSPPSDL \
  /mnt/SDCARD/plumos/emulators/ppsspp-vanilla/bin/PPSSPPSDL \
  /mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-sdlgl-a30 \
  /mnt/SDCARD/plumos/emulators/red_viper/bin/red-viper-a30; do
  [ -e "$p" ] && echo "$p"
done | sort' > artifacts/a30-core-inventory/deployed-runtime-paths-20260618.txt

./scripts/generate-fe-verification-targets.py \
  --deployed-cores artifacts/a30-core-inventory/deployed-libretro-cores-20260618.txt \
  --deployed-paths artifacts/a30-core-inventory/deployed-runtime-paths-20260618.txt
```
