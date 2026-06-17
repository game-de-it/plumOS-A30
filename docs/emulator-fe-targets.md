# FE 実行可能ターゲット一覧

この一覧は、plumOS でビルドした core、実機にデプロイ済みの runtime、FE から選択できる
launch profile を突き合わせたものです。動作確認対象は `target_for_verification=yes` の行だけです。

## 出力ファイル

- `docs/emulator-fe-libretro-targets.tsv`
  - RetroArch (`RA`) と PicoArch (`PICO`) の FE 実行可能 profile。
  - PicoArch は FE が RetroArch profile から自動追加する companion profile も含みます。
- `docs/emulator-fe-standalone-targets.tsv`
  - `standalone:*` と、libretro ではない `pyxel:a30` の FE 実行可能 profile。

## 判定ルール

- `built_core=yes`: `docs/libretro-built-cores.tsv` に plumOS build 済み core として存在する。
- `deployed_core=yes`: 実機 `/mnt/SDCARD/plumos/retroarch/cores/*_libretro.so` に存在する。
- `fe_selectable=yes`: enabled system の launch profile、または FE が追加する PicoArch companion。
- `fe_executable=yes`: FE が起動計画を作れる runtime/core/launcher が実機に存在する。
- `target_for_verification=yes`: build、deploy、FE 実行可能条件がすべて揃っている。
- `verification_status`: `docs/emulator-runtime-verification.tsv` の現在の検証状態。

PicoArch companion は FE 側で `retroarch:*` profile から自動追加されます。ただし
`freeintv`、`mednafen_pce`、`nekop2`、`np2kai`、`quasi88`、`tgbdual` は既知問題があるため自動追加対象から外しています。

N64 は Class C の実験対象でしたが、性能と描画経路の都合で正式な検証対象から外しました。
stockOS 由来の N64 core は plumOS の build/deploy 一覧に含めません。

## 再生成手順

実機の deployed 状態を回収してから TSV を生成します。

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
