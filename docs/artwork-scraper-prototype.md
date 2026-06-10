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

将来 `systems.json` に入れる場合の例:

```json
{
  "id": "nes",
  "scraper": {
    "enabled": true,
    "crc_workers": { "default": 1, "bulk": 2, "max": 5 },
    "download_workers": { "default": 2, "bulk": 3, "max": 4 }
  }
}
```

実機で CRC worker 数を測る入口は `scripts/benchmark-a30-crc-workers.sh` です。ROM file 名は
出力せず、system、worker 数、処理件数、秒数だけを出します。

```sh
A30_TARGET=root@192.168.10.165 \
  scripts/benchmark-a30-crc-workers.sh --system nes --workers "1 2 3 4 5" --limit 100
```

測定結果は system 別 policy の根拠として `artifacts/` に保存します。

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
6. download に成功したら保存する。失敗したら `no_match` にする。
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

- `no_match` の内訳を見て、CRC miss と thumbnail miss を分ける。
- `--loose-index` が必要な system と不要な system を切り分ける。
- 直列 HEAD/download が遅い場合は、shell prototype で並列数を決めてから FE queue へ移す。
- system 別 CRC worker 数は、A30 実機で `scripts/benchmark-a30-crc-workers.sh` を使って
  測定してから `systems.json` policy へ反映する。
- ダウンロード済み PNG の resize/cache は、原本 `Images/<system_id>` を壊さず
  `/mnt/SDCARD/plumos/cache/frontend/render-cache` のような破棄可能 cache に分ける。
- 実機では network service の有無、空き容量、途中中断時の `.tmp` cleanup を確認する。
