# artwork scraper prototype

この文書は、FE へ直接組み込む前に shell script で thumbnail scraping の照合順と保存先を
検証するための設計メモです。実装試作は `scripts/prototype-thumbnail-scraper.sh` です。

## 目的

- ROM の CRC が libretro database に一致する場合は、canonical game name から thumbnail を取得する。
- CRC が一致しない ROM も多い前提で、ROM file name からの fallback を試す。
- 取得した画像とユーザーが手で置く画像の保存先は 1 つにする。
- FE にはまだ background download、progress UI、retry UI を入れない。

## 正式保存先

scraper の保存先、ユーザー手動配置、frontend lookup はすべて以下へ揃えます。

```text
/mnt/SDCARD/Images/<system_id>/<ROM alias root からの relative stem>.png
```

例:

```text
Roms/FC/Nintendo/Mario.nes
=> Images/nes/Nintendo/Mario.png

Roms/GB/Dracula Densetsu.gb
=> Images/gb/Dracula Densetsu.png
```

## 試験対象

現時点の host-side 試験対象:

```text
artifacts/reference/nes
artifacts/reference/gb
```

`artifacts/reference/nes` には `.nes` のほか `.fds` と `.zip` も含まれます。`.fds` は
`system_id=fds` として扱い、保存先も `Images/fds` にします。`.zip` は最初に見つかった
`.nes` / `.fds` / `.gb` member を一時展開して CRC を取ります。

## 参照元

| `system_id` | libretro playlist | DAT source |
| --- | --- | --- |
| `nes` | `Nintendo - Nintendo Entertainment System` | `metadat/no-intro/Nintendo - Nintendo Entertainment System.dat` |
| `fds` | `Nintendo - Family Computer Disk System` | `metadat/no-intro/Nintendo - Family Computer Disk System.dat` |
| `gb` | `Nintendo - Game Boy` | `metadat/no-intro/Nintendo - Game Boy.dat` |

thumbnail 種別は初期値を `Named_Boxarts` にします。試作 script では `--kind Named_Snaps` /
`--kind Named_Titles` も選べます。

## 照合順

1. 既に保存先 PNG がある場合は `exists` として何もしない。
2. ROM payload の CRC32 を取る。
3. system 別 DAT index から `crc -> canonical game name` を引く。
4. canonical game name の PNG が thumbnail server に存在すれば `crc-exact`。
5. ROM file stem の PNG が thumbnail server に存在すれば `name-exact`。
6. `--loose-index` が指定された場合だけ、大きい thumbnail directory index を取得し、
   bracket/region/revision を落とした normalized name で `crc-loose` / `name-loose` を試す。
7. どれも失敗した場合は `no_match`。`--fetch` 時だけ negative cache に記録する。

通常 dry-run は negative cache を更新しません。

`--candidate-report <path>` を指定した場合、`no_match` になった ROM について thumbnail
directory index から近い候補を順位付き TSV で出力します。候補は確認用であり、曖昧な一致を
自動保存しません。CRC 由来の canonical 名がある場合はそれを優先し、なければ ROM file stem
から候補を作ります。既定では ROM ごとに最大 5 件、score `60` 以上を出力します。

候補 TSV の列:

```text
status system source score crc rom_path dest_path query_name thumbnail_name url
```

## 実行例

照合だけ:

```sh
scripts/prototype-thumbnail-scraper.sh --dry-run --limit 20
```

GB だけ照合:

```sh
scripts/prototype-thumbnail-scraper.sh --dry-run --system gb --limit 20
```

安全な一時 directory へ実取得:

```sh
scripts/prototype-thumbnail-scraper.sh \
  --fetch \
  --limit 5 \
  --image-root /tmp/plumos-thumb-images
```

未マッチ ROM の候補一覧を出す:

```sh
scripts/prototype-thumbnail-scraper.sh \
  --dry-run \
  --retry-negative \
  --candidate-report artifacts/thumb-scraper/reports/candidates.tsv
```

A30 の正式 root へ取得する場合:

```sh
PLUMOS_SDCARD_ROOT=/mnt/SDCARD \
  scripts/prototype-thumbnail-scraper.sh --fetch --system nes
```

## 今後 FE へ入れる前の確認点

- `no_match` の内訳を見て、CRC miss と thumbnail miss を分ける。
- `--loose-index` が必要な system と不要な system を切り分ける。
- 直列 HEAD/download が遅い場合は、shell prototype で並列数を決めてから FE queue へ移す。
- ダウンロード済み PNG の resize/cache は、原本 `Images/<system_id>` を壊さず
  `/mnt/SDCARD/plumos/cache/frontend/render-cache` のような破棄可能 cache に分ける。
- 実機では network service の有無、空き容量、途中中断時の `.tmp` cleanup を確認する。
