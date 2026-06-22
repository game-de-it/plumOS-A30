# libretro core version inventory

調査日: 2026-06-18

この文書は、Onion 側にある libretro core の version/provenance と、plumOS 側 recipe の
対応状況を固定するためのメモです。詳細な棚卸し表は
`docs/libretro-core-version-inventory.tsv` を正本にします。Onion の実績時期から解決した
upstream source commit は `docs/onion-libretro-source-lock.tsv` を正本にします。

## 入力

- Onion repository: `artifacts/onion/Onion`
  - commit: `07505ea58c7bba698d6b9220ff43946a43cac76b`
  - prebuilt core: `static/build/RetroArch/.retroarch/cores/*_libretro.so`
- Onion builder repository: `artifacts/onion/Miyoo-Mini-Retroarch-builder`
  - commit: `9efb4acbe95a9e7a370487d7fc1b9b7d40fda9c0`
  - recipe: `cores-onionos`, `cores-onionos-special`
- plumOS recipe: `docker/plumos-toolchain/libretro-core-recipes.tsv`

## 方針

- Onion prebuilt に存在し、plumOS recipe にもある core は `prefer_onion_version` とする。
  Onion の `.info` にある `display_version`、Onion repository 上の prebuilt binary
  最終更新 commit、Onion builder recipe の `repo/ref` を優先調査対象にする。
  実際の plumOS build では Onion prebuilt binary を採用せず、
  `onion-libretro-source-lock.tsv` で解決した source commit を Docker toolchain で build する。
- Onion prebuilt に存在し、plumOS recipe に無い core は `missing_from_plumos` とする。
  取り込み可否、source provenance、A30 実用性を追加調査する。
- Onion prebuilt に存在せず、plumOS recipe にだけある core は `plumos_only_latest` とする。
  現時点では QuickNES のみで、upstream `HEAD` を採用候補として build する。

## 2026-06-13 時点の集計

| policy | count | note |
| --- | ---: | --- |
| `prefer_onion_version` | 96 | Onion prebuilt と plumOS recipe の両方にある。 |
| `missing_from_plumos` | 14 | Onion prebuilt にあるが plumOS recipe には未登録。 |
| `plumos_only_latest` | 1 | QuickNES。Onion prebuilt には無いが plumOS では残す。 |

## Onion prebuilt-only core の整理

`missing_from_plumos` の 14 件は、現時点では通常 FE 候補ではありません。多くは
確認済みの plumOS route で代替できるもの、plumOS がまだ system として公開していない
game/application core、または KM/custom prebuilt 派生で、source provenance と recipe 化の
判断を別途必要とするものです。以下に、`libretro-core-recipes.tsv` へ取り込まなかった
現時点の理由を残します。

| core | Onion 側の役割 | plumOS の現時点判断 |
| --- | --- | --- |
| `a5200` | Atari800 2.0.2 ベースの Atari 5200 専用 core。 | Atari 5200 / Atari 8-bit は既に `atari800` で FE 公開済みで、RA/PICO とも system-specific core option を使うため未取り込み。ただし 5200 専用の単純な route として、後日評価する価値はある。 |
| `arduous` | Arduboy core。source hint: `https://github.com/libretro/arduous.git`, CMake。 | plumOS に Arduboy system が未定義のため未取り込み。Arduboy の system 定義、ROM directory、合法 test content を選ぶ場合に追加検討する。 |
| `dosbox_pure_0.9.7` | DOSBox-Pure 0.9.7 の旧 prebuilt。 | plumOS は既に通常 `dosbox_pure` を build し DOS route として公開済み。明確な regression が無い限り、旧版固定 route は持たない。 |
| `fbalpha2012_cps3` | RAM 制約機器向けの CPS-3 専用 FB Alpha 2012 variant。source hint: `https://github.com/libretro/fbalpha2012_cps3.git`, `svn-current/trunk`, `makefile.libretro`。 | CPS3 は確認済みの `fbneo` と通常 `fbalpha2012` route で対応済み。Onion の `.info` でも RAM 節約用の特殊 variant で、通常は FBNeo 推奨と説明されているため未取り込み。 |
| `gong` | ROM extension を持たない self-contained Pong clone core。source hint: `https://github.com/libretro/gong.git`。 | plumOS FE は ROM directory 起点の launcher なので、通常の emulator route としては扱いにくい。採用するなら ROM system ではなく Apps/game entry として再検討する。 |
| `km_duckswanstation_xtreme_amped` | KMFDManic Duck/SwanStation 系 PSX variant。hardware renderer 要求あり。 | PlayStation は `pcsx_rearmed` の RA/PICO/standalone route が確認済み。この variant は近代的な GL/Vulkan/D3D 系 hardware rendering 前提で、A30 向けとして優先しない。 |
| `km_mame2003_xtreme` | KMFDManic MAME 2003 Xtreme arcade variant。 | Arcade は既に `fbneo`, `fbalpha2012`, `mame2000`, `mame2003_plus` などの実用 route を持つ。KM source provenance と ROM-set 方針は別途 arcade 方針として判断する。 |
| `km_puae_xtreme_amped` | KMFDManic P-UAE Xtreme Amped Amiga variant。 | Amiga は確認済みの `puae` RA/PICO route で公開済み。custom performance variant として、source provenance と A30 比較が必要になるため未取り込み。 |
| `km_superbroswar` | Super Bros War game-engine core。 | plumOS に Super Bros War の system/asset 方針がないため未取り込み。専用 game-engine entry が必要になった時に検討する。 |
| `mba_mini` | Arcade/FBA/MAME hybrid mini core。 | 既存の Arcade/CPS/Neo Geo route と用途が重複し、独自 ROM-set 前提もある。source provenance と実用的な ROM-set 価値を別途判断する必要がある。 |
| `puae2021` | `libretro-uae` 2.6.1 付近の旧 PUAE 2021 branch。source hint: `https://github.com/libretro/libretro-uae.git`, checkout `2.6.1`。 | plumOS は通常 `puae` を build し、Amiga route として確認済み。現行 `puae` に regression が出た場合の fallback 候補として扱う。 |
| `puzzlescript` | PuzzleScript engine。source hint: `https://github.com/nwhitehead/pzretro.git`。 | plumOS に PuzzleScript system が未定義のため未取り込み。system 定義、`.pz` ROM directory 方針、合法 test content が揃った場合に追加する。 |
| `sameduck` | Mega Duck / Cougar Boy core。source hint: `https://github.com/libretro/sameduck.git`, branch `SameDuck-libretro`, subdir `libretro`。 | Mega Duck system が未定義で、検証 content も未選定のため未取り込み。 |
| `uae4arm` | 古い軽量 Amiga core。source hint: `https://github.com/libretro/uae4arm-libretro.git`。 | Amiga は `puae` が確認済み route。Onion の `.info` でも、P-UAE が使えない/重すぎる弱い環境向け fallback と位置づけられているため通常採用しない。 |

## 再生成

```sh
./scripts/inventory-libretro-core-versions.py > docs/libretro-core-version-inventory.tsv
./scripts/resolve-onion-libretro-source-lock.py > docs/onion-libretro-source-lock.tsv
./scripts/apply-onion-source-lock-to-recipes.py --output docker/plumos-toolchain/libretro-core-recipes.tsv
```

Onion builder recipe だけに存在する core も含めて見たい場合は、以下を使います。

```sh
./scripts/inventory-libretro-core-versions.py --include-builder-only
```

TSV の主な列:

- `onion_display_version`: Onion 同梱 `.info` の `display_version`。
- `onion_binary_sha256`: Onion prebuilt `.so` の SHA256。
- `onion_binary_commit` / `onion_binary_date`: Onion repository 内で当該 `.so` が最後に
  更新された commit。
- `onion_builder_repo` / `onion_builder_ref`: Onion builder recipe の source 情報。
- `plumos_repo` / `plumos_ref`: plumOS が build に使う source 情報。

`onion-libretro-source-lock.tsv` の主な列:

- `onion_binary_date`: Onion repository で当該 prebuilt core が最後に更新された日時。
- `embedded_build_date`: core binary 内の文字列から検出できた build/compile 日時候補。
- `lock_date`: source commit を解決する基準日時。`embedded_build_date` が取れる場合はそれを
  優先し、無い場合は `onion_binary_date` を使う。
- `resolved_source_commit`: `onion_builder_repo/ref`、または builder 情報が無い場合の
  plumOS recipe repo/ref から、`lock_date` 以前の最新 commit として解決した source commit。
- `resolution_status`: `resolved` の行だけを recipe pin に使う。

## source lock の注意点

binary 内の文字列には game database や ROM 名に由来する日付も混ざるため、
`embedded_build_date` は無条件に信用しません。現在の resolver は、Onion binary の
更新日時より 180 日以上古い embedded date を source lock の基準に使わず、
`onion_binary_date` 側へ戻します。これは `fbneo`、`mame2000`、`mame2003_plus`、
`tyrquake` で必要でした。

一部 core は Onion の採用時期だけでは buildable な source を一意に復元できないため、
`onion-libretro-source-lock.tsv` の `notes` に `source_override` と理由を残しています。
主な例外は以下です。

- `fake08`: Onion commit message が明示していた `aebd6b9` を採用。
- `fbneo`: Onion 時期の master では recipe の libretro Makefile path が残らないため、
  libretro Makefile が存在する直近 commit を採用。
- `pcsx_rearmed`: Onion window の `Makefile.libretro` は unmaintained stop になっているため、
  buildable な libretro fork commit を採用。
- `scummvm`: Onion window の source には現在の libretro backend が無いため、
  buildable な libretro backend commit を採用。
- `tic80`: `nesbox/TIC-80` の 0.80-era source は現在取得不能な submodule を参照するため、
  libretro wrapper repository を採用。libretro wrapper の CMake default では
  `BUILD_STATIC=OFF` になり Lua script backend が静的登録されないため、plumOS build では
  `-DBUILD_STATIC=ON` を追加する。

2026-06-18 時点で、この source lock を `docker/plumos-toolchain/libretro-core-recipes.tsv`
へ反映し、`PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4`
で全 97 recipe の source build が `built=97`, `failed=0`, `skipped=0` になっています。
