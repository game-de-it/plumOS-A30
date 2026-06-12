# libretro core version inventory

調査日: 2026-06-13

この文書は、Onion 側にある libretro core の version/provenance と、plumOS 側 recipe の
対応状況を固定するためのメモです。詳細な棚卸し表は
`docs/libretro-core-version-inventory.tsv` を正本にします。

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
