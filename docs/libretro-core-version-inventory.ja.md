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
