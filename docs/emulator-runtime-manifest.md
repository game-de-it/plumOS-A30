# emulator runtime manifest

更新日: 2026-06-19

この文書は、plumOS の FE から実行できる emulator / libretro core / Pyxel runtime の
採用 manifest を固定するためのメモです。機械可読な正本は
`docs/emulator-runtime-manifest.tsv` です。

## 位置づけ

`docs/emulator-runtime-manifest.tsv` は、次の条件を満たす launch profile だけを列挙します。

- FE から実行できる。
- `verification_status` が `pass`、`pass_init`、`untested` のいずれか。
- `retired` は通常候補から外したものとして除外する。

`pass_init` と `untested` は、現時点で追加の合法 ROM / BIOS / 実機操作がないと進められない
候補です。失敗扱いではなく、採用候補または保留候補として残します。

## 現在の集計

| item | count |
| --- | ---: |
| runtime profile | 220 |
| `pass` | 168 |
| `pass_init` | 46 |
| `untested` | 6 |
| RetroArch profile | 111 |
| PicoArch profile | 103 |
| standalone profile | 5 |
| Pyxel profile | 1 |
| unique libretro core used by FE candidates | 77 |

`fail_*` は採用 manifest には残しません。性能不足、代替あり、入力/映像/音声問題が残るものは
`docs/emulator-runtime-verification.tsv` 側で `retired` にして、FE 通常候補から外します。

## 正本

| file | role |
| --- | --- |
| `docker/plumos-toolchain/libretro-core-recipes.tsv` | libretro core の full build recipe。repo/ref/makefile/make_args を固定する。 |
| `docs/onion-libretro-source-lock.tsv` | Onion 採用時期から解決した source commit。Onion prebuilt binary は採用しない。 |
| `docs/libretro-core-version-inventory.tsv` | Onion prebuilt / Onion builder / plumOS recipe の対応棚卸し。 |
| `docs/libretro-built-cores.tsv` | 最新 all build manifest から更新した build 済み 97 core の inventory。 |
| `docs/emulator-fe-libretro-targets.tsv` | FE から実行可能な libretro/RA/PICO profile の検証対象表。 |
| `docs/emulator-fe-standalone-targets.tsv` | FE から実行可能な standalone/Pyxel profile の検証対象表。 |
| `docs/emulator-runtime-manifest.tsv` | FE 採用候補 profile と source/ref/build option を結合した最終 manifest。 |

## 再生成

libretro core を rebuild した後は、まず build manifest から built core inventory を更新します。

```sh
./scripts/update-libretro-built-core-inventory.py \
  --manifest dist/plumos-libretro-cores-onion-lock-all/docs/manifest.txt \
  --output docs/libretro-built-cores.tsv
```

FE target TSV を更新した後、採用 manifest を再生成します。

```sh
./scripts/generate-emulator-runtime-manifest.py \
  --output docs/emulator-runtime-manifest.tsv
```

manifest の `source_ref` は、libretro core では最新 build manifest の `commit` を優先します。
standalone / Pyxel は build script の既定 `*_REF` を読み取ります。RetroArch と PicoArch は
各 libretro profile の `frontend_runtime` として記録し、core の source/ref とは分けます。

## 注意点

- PicoArch は原則として `/mnt/SDCARD/plumos/retroarch/cores` の libretro core を使います。
  ただし `picoarch:fceumm` は PicoArch package 同梱の互換 core が存在する場合に優先します。
- `quicknes` は Onion prebuilt に無い plumOS 独自採用 core なので
  `version_policy=plumos_only_latest` です。
- Onion prebuilt に存在しても plumOS recipe に無い core は、現時点では FE 採用 manifest に
  入れません。source provenance と A30 実用性を確認できたものだけ recipe へ追加します。
