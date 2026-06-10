# artwork scraper prototype

この文書は、FE へ直接組み込む前に shell script で thumbnail scraping の照合順と保存先を
検証するための設計メモです。実装試作は `scripts/prototype-thumbnail-scraper.sh` です。

## 目的

- ROM の CRC が libretro database に一致する場合は、canonical game name から thumbnail を取得する。
- CRC が一致しない ROM は通常 `no_match` にする。filename fallback は明示的な候補確認用
  option に限定し、自動保存しない。
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

## 再実行ポリシー

2 回目以降の scraper は、既存 thumbnail をユーザーの意思として優先します。

- ROM ごとにまず frontend と同じ lookup 順で `/mnt/SDCARD/Images/<system_id>` を見る。
- `png`, `jpg`, `jpeg`, `webp` のいずれかが見つかった場合は `exists` として終了する。
- `exists` では CRC 計算、DAT lookup、thumbnail index 取得、download を行わない。
- subdirectory を保った画像を flat 画像より優先する。
- scraper が取得した PNG とユーザーが置いた画像は同じ扱いにし、default では上書きしない。
- 既存画像を再取得する操作は、明示的な `Force refresh` 相当の option に限定する。
- ユーザー手動画像まで置き換える操作はさらに別 option にし、default では無効にする。

画像が存在しない ROM だけ CRC queue に入れます。FE 組み込み時は
`/mnt/SDCARD/plumos/state/frontend/artwork-scraper-state.json` のような state に、ROM の
`system_id`, relative path, size, mtime, ctime, kind, CRC, status, checked_at を残します。
同じ size/mtime/ctime で過去に `no_match` だった ROM は、DAT cache が更新された場合または
ユーザーが `Retry missing thumbnails` を選んだ場合だけ再 CRC します。`download_failed` は
network 一時失敗の可能性があるため、短い backoff または手動 retry で再試行します。
試作 script の negative cache も、CRC ではなく `system_id`, kind, relative path, size, mtime, ctime
を key にして、変化していない未マッチ ROM では CRC 前に `negative_cached` を返します。
同名・同サイズで ROM を再配置しても、通常は mtime または ctime が変わるため CRC 対象になります。
timestamp を完全に保持する特殊なコピー経路に備え、FE には negative cache を無視する
`Retry missing thumbnails` を用意します。

## FE 組み込み方針

scraper の対象範囲と frontend の thumbnail 表示範囲は分けます。scraper 対象外の system でも、
ユーザーが `/mnt/SDCARD/Images/<system_id>/...` に置いた thumbnail は通常通り表示します。

通常 scraper は、単純な ROM payload CRC で libretro database と照合しやすい cartridge 系を
対象にします。

```text
nes
fds
sfc
gb
gbc
gba
megadrive
mastersystem
gamegear
sega32x
pcengine
n64
ngp
ngpc
wonderswan
wonderswancolor
msx
```

CD 系は実機上で CRC 照合だけでも重くなりやすく、track / CHD / ISO / cue / m3u / pbp
などの代表 file 判断も必要になるため、初期 scraper 対象外にします。

```text
segacd
pcenginecd
psx
psp
dreamcast
saturn
```

PC 系 disk image は、D88/HDI/DIM/XDF/m3u などの代表 file 判断と DAT 種別の方針が必要なため、
初期 scraper 対象外にします。

```text
pc88
pc98
x68000
```

Arcade / romset 系は zip 全体 CRC ではなく内部 ROM member や romset version 依存の照合が
必要なため、初期 scraper 対象外にします。

```text
arcade
fbneo
mame2003plus
cps1
cps2
cps3
neogeo
```

file CRC で game を特定しにくい system も scraper 対象外にします。

```text
dos
easyrpg
pico8
scummvm
openbor
ports
tic80
```

CRC miss の扱い:

- 通常動作では `no_match` として終了する。
- 日本語 file stem は filename candidate 探索をしない。
- filename 由来の候補探索は、ユーザーが明示的に選ぶ確認用 option に限定する。
- 候補探索で見つかった画像は自動保存せず、ユーザーが選択したものだけ保存する。

## 並列度

scraper は CRC queue と download queue を分けます。

初期値:

```text
CRC workers default: 1
CRC workers bulk:    2
PNG download default: 2
PNG download bulk:    3
PNG download hard cap: 4
```

CRC workers は system ごとに override できるようにします。小さい ROM が多い system は
実機検証後に上げられますが、N64 のように ROM が大きい system は `1` のままにできます。
未検証 system は `default=1`, `bulk=2`, `max=2` を使います。検証用の上限探索は
`max=5` までに抑え、FE の通常UIで無制限な値を選ばせません。

`package/frontend/plumos/config/frontend/systems.json` の `scraper` field を scraper 対象可否の
正本にします。初期対象 system は `enabled=true`, `reason=simple_rom_crc` とし、未検証なので
`crc_workers.max=2` に抑えます。実機 benchmark 後に system ごとの `max` を上げます。
`scraper.extensions` は初期 scraper が CRC 対象にする拡張子です。system 全体の
`extensions` に `7z` や disk image が含まれる場合でも、ここに無い拡張子は scraping しません。

有効 system の例:

```json
{
  "id": "nes",
  "scraper": {
    "enabled": true,
    "reason": "simple_rom_crc",
    "extensions": ["nes", "unf", "unif", "zip"],
    "crc_workers": { "default": 1, "bulk": 2, "max": 2 },
    "download_workers": { "default": 2, "bulk": 3, "max": 4 }
  }
}
```

対象外 system は `scraper.enabled=false` とし、理由は以下を使います。

```text
cd_media_crc_expensive
pc_disk_image_policy_pending
arcade_romset_policy_pending
not_single_rom_crc
```

実機で CRC worker 数を測る入口は `scripts/benchmark-a30-crc-workers.sh` です。ROM file 名は
出力せず、system、worker 数、処理件数、秒数だけを出します。

```sh
A30_TARGET=root@192.168.10.165 \
  scripts/benchmark-a30-crc-workers.sh --system nes --workers "1 2 3 4 5" --limit 100
```

測定結果は system 別 policy の根拠として `artifacts/` に保存します。

## 実機 runner

`package/frontend/plumos/bin/plumos-thumbnail-scraper` は、実機上で scraper policy と既存
thumbnail skip の件数を確認し、FE Apps からも呼べる package runner です。
`--plan` / `--dry-run` では CRC 計算や network access を行わず、`--fetch` では既存 thumbnail が
ない ROM だけ CRC -> DAT lookup -> thumbnail index lookup -> PNG download を行います。
runner は標準出力と log に ROM file 名を出さず、system ごとの集計だけを出します。

主な用途:

```sh
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --list-systems
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --system gb
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --all --limit 50
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --system gb --limit 20
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --all --limit 20
```

環境変数:

- `PLUMOS_SDCARD_ROOT`: default `/mnt/SDCARD`
- `PLUMOS_ROOT`: default `$PLUMOS_SDCARD_ROOT/plumos`
- `PLUMOS_SYSTEMS_JSON`: default `$PLUMOS_ROOT/config/frontend/systems.json`
- `PLUMOS_SCRAPER_SOURCES`: default `$PLUMOS_ROOT/config/frontend/scraper-sources.tsv`
- `PLUMOS_ROM_ROOT`: 指定時はこの root だけを見る。未指定時は `/mnt/SDCARD/Roms` と
  `/mnt/SDCARD/roms` を frontend と同じように見る
- `PLUMOS_IMAGE_ROOT`: default `$PLUMOS_SDCARD_ROOT/Images`
- `PLUMOS_THUMBNAIL_STATE_DIR`: default `$PLUMOS_ROOT/state/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_CACHE_DIR`: default `$PLUMOS_ROOT/cache/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_PRELOAD_DIR`: default `$PLUMOS_ROOT/share/frontend/artwork-scraper`
- `PLUMOS_THUMBNAIL_FETCH_TIMEOUT`: default `45` 秒

`--system <id>` は disabled system の場合、理由を出して exit code `2` を返します。`--all` は
enabled system だけを plan します。出力は TSV で、ROM file 名は出しません。

plan 出力の列:

```text
status system enabled reason aliases_seen rom_candidates existing_thumbnails missing_thumbnails crc_workers download_workers
```

`rom_candidates` は `systems.json` の `scraper.extensions` に一致した file 数です。
`existing_thumbnails` は frontend と同じ lookup 順で `/mnt/SDCARD/Images/<system_id>` を見て、
subdirectory 画像、flat fallback 画像のどちらかが存在した件数です。`missing_thumbnails` だけが、
次段階の CRC/DAT/download queue に入る対象です。
runner は system ごとに既存画像の一時 lookup を 1 回だけ作り、ROM ごとの repeated `find` を避けます。
`--fetch` 中に新しく保存できた画像もこの lookup に追記し、同一 run 内の重複処理を避けます。

fetch 出力の列:

```text
status system enabled reason aliases_seen rom_candidates existing_thumbnails missing_thumbnails crc_checked crc_matched downloaded no_match crc_miss thumbnail_miss download_failed invalid_png skipped_zip skipped_tool
```

`--fetch` は `PLUMOS_ROOT/bin` を PATH 先頭に追加し、plumOS 同梱 BusyBox の `crc32`, `wget`,
`unzip` などを使います。DAT/index はまず `PLUMOS_THUMBNAIL_PRELOAD_DIR` を見て、無い場合だけ
`PLUMOS_THUMBNAIL_CACHE_DIR` へ取得します。thumbnail PNG は libretro thumbnail server の
HTTP URL から取得し、`/mnt/SDCARD/Images/<system_id>/<relative stem>.png` へ保存します。
`--fetch --all` でも、ROM 候補が無い system や既存 thumbnail だけで足りる system では
DAT/index を取得しません。最初の missing thumbnail が見つかった時だけ、その system の DAT/index を
用意します。
`no_match` は `crc_miss + thumbnail_miss` の合計です。`crc_miss` は CRC が DAT に無い場合、
`thumbnail_miss` は CRC は一致したが thumbnail index に同名 PNG が無い場合です。
network 待ちで UI 操作を長時間止めないよう、`wget` / `curl` には
`PLUMOS_THUMBNAIL_FETCH_TIMEOUT` の timeout をかけます。

FE の START -> Apps には `Scraping` 入口を置きます。`Scraping` 画面では
`System < ALL >` または `System < NES >` のように対象 system を選び、A で実行します。
対象は `scraper.enabled=true` かつ現在の ROM 数が 1 以上の system だけです。画面を開く時に
library scan を行い、FE 再起動なしで TOP/Graphic の ROM 数と Scraping 対象を更新します。
実行中は `Scraping Running` 画面を表示し、内部では対象 system ごとに
`plumos-thumbnail-scraper --system <id>` と
`plumos-thumbnail-scraper --fetch --system <id>` を順に実行します。完了後は自動で
`Scraping Results` へ遷移します。`Scraping Results` は直近 1 回分の
`frontend-apps-latest.log` だけを表示するため、いま実行した結果かどうかを判断できます。
詳細 log は `/mnt/SDCARD/plumos/logs/frontend-apps.log` と runner log にも残します。

- `Scraping`: system 選択、plan -> fetch 実行、最新結果表示への入口。
- `Scraping Results`: 最新 log を読みやすい複数行にまとめて表示する。
  `Thumbnail Plan` は `reason`、`aliases seen`、`ROMs`、`existing`、
  `missing`、`CRC workers`、`DL workers` を表示する。
  `Fetch Thumbnails` は `reason`、`aliases seen`、`ROMs`、`existing`、
  `missing`、`CRC checked`、`CRC matched`、`downloaded`、`no match`、
  `CRC miss`、`image miss`、`download failed`、`invalid PNG`、`skipped zip`、
  `skipped tool` を表示する。画面は上下で 1 行スクロール、左右でページ移動できる。
  各項目の意味と対処は `docs/thumbnail-scraping-results.md` にまとめる。

source 定義は `package/frontend/plumos/config/frontend/scraper-sources.tsv` です。列は
`system_id`, `libretro_playlist`, `libretro_dat_path` です。`systems.json` の
`scraper.enabled=true` system だけを prefetch/fetch 対象にします。

## 事前取得 cache

`scripts/prefetch-thumbnail-scraper-cache.sh` は host/build 用の事前取得入口です。libretro DAT を
`crc -> canonical game name` の TSV に変換し、thumbnail directory index を
`canonical game name -> encoded png href` の TSV に変換します。generated data は git に入れず、
frontend build 時に `dist/plumos-frontend/plumos/share/frontend/artwork-scraper/` へ入れます。

```sh
scripts/prefetch-thumbnail-scraper-cache.sh \
  --output dist/plumos-frontend/plumos/share/frontend/artwork-scraper
```

`docker/plumos-toolchain/scripts/build-frontend.sh` は
`PLUMOS_PREFETCH_THUMBNAIL_CACHE=auto` を default とし、network が使える場合はこの cache を
事前生成します。`0` / `false` / `skip` で省略、`1` / `true` で失敗を build error にできます。

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

1. 既存 thumbnail を frontend と同じ lookup 順で探す。見つかった場合は `exists` として何もしない。
2. 既存 thumbnail がなく、同じ size/mtime/ctime の negative cache がある場合は
   `negative_cached` として何もしない。
3. ROM payload の CRC32 を取る。
4. system 別 DAT index から `crc -> canonical game name` を引く。
5. CRC が一致した場合、canonical game name から URL を確定し、直接 download を試す。
6. download に成功したら保存する。失敗したら `download_failed` または `invalid_png` にする。
7. CRC が一致しない場合、通常動作では `no_match` にする。
8. 明示 option が指定された場合だけ、ROM file stem 由来の `name-exact` や
   `candidate-report` を試す。
9. `--loose-index` が指定された場合だけ、大きい thumbnail directory index を取得し、
   bracket/region/revision を落とした normalized name で `crc-loose` / `name-loose` を試す。
10. どれも失敗した場合は `no_match`。`--fetch` 時だけ negative cache に記録する。

通常 dry-run は negative cache を更新しません。

`exists` と `negative_cached` は CRC 計算前に返るため、report の CRC 欄は `-` になる場合があります。

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

- `crc_miss` / `thumbnail_miss` の内訳を見て、system ごとに URL 確定率を確認する。
- `--loose-index` が必要な system と不要な system を切り分ける。
- 直列 HEAD/download が遅い場合は、shell prototype で並列数を決めてから FE queue へ移す。
- system 別 CRC worker 数は、A30 実機で `scripts/benchmark-a30-crc-workers.sh` を使って
  測定してから `systems.json` policy へ反映する。
- ダウンロード済み PNG の resize/cache は、原本 `Images/<system_id>` を壊さず
  `/mnt/SDCARD/plumos/cache/frontend/render-cache` のような破棄可能 cache に分ける。
- 実機では network service の有無、空き容量、途中中断時の `.tmp` cleanup を確認する。
