# libretro core / system inventory

2026-06-13 の `PLUMOS_CORE_FILTER=all FAIL_ON_CORE_ERROR=1 LIBRETRO_CORE_BUILD_CONCURRENCY=4 ./scripts/docker-build.sh libretro-cores`
で生成できた libretro core と、`systems.json` へ反映するための候補表です。

- `docs/libretro-built-cores.tsv`: 生成済み 97 core の一覧。`*_libretro.info`、build manifest、候補 system id をまとめる。
- `docs/libretro-system-core-map.tsv`: system 単位の候補表。`rom_dir`、対応拡張子、default core、候補 core をまとめる。

`status` の意味:

- `existing`: 現在の `package/frontend/plumos/config/frontend/systems.json` に同じ system/core 対応がある。
- `add_core`: system は既にあるが、今回生成できた追加 core を launch profile 候補へ足せる。
- `add_system`: system 自体を新規追加する候補。BIOS、実機性能、入力、data layout の確認が必要なものを含む。
- `contentless`: ROM ファイルなしで起動できる core。通常の ROM scanner にそのまま入れるかは別途 UX を決める。

ROM directory は、既存 system では現在の `systems.json` の primary directory alias を優先しました。
新規 system では Onion の ROM folder 名が確認できるものを優先し、確認できないものは plumOS 候補名として
大文字の system 名を置いています。
