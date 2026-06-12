# サムネイルスクレイピング結果

この文書は、plumOS の `Scraping Results` 画面に出る各項目の意味と、
失敗が出た場合の対処をまとめます。

`Scraping Results` は直近 1 回のスクレイピング結果だけを表示します。
画面は上下で 1 行スクロール、左右でページ移動できます。

## Plan Result

`Thumbnail Plan` は、実際のダウンロード前に ROM と既存サムネイルを数えます。

```text
nes
reason simple_rom_crc
aliases seen 1
ROMs 50
existing 8
missing 42
CRC workers 1/2/2
DL workers 2/3/4
----------------
```

- `reason`: その system がスクレイピング対象になっている理由です。
- `aliases seen`: 見つかった ROM directory alias 数です。
- `ROMs`: スクレイピング対象として見つかった ROM 数です。
- `existing`: すでに `/mnt/SDCARD/Images/<system>/` に画像がある ROM 数です。
- `missing`: まだ画像がない ROM 数です。Fetch では基本的にこの数が処理対象です。
  `Existing < Replace >` または `--replace-existing` の場合は、既存画像も置き換え対象になるため、
  `missing` は「今回の処理キュー数」として表示されます。
- `CRC workers`: CRC チェックの並列数です。`default/bulk/max` の順です。
- `DL workers`: 画像ダウンロードの並列数です。`default/bulk/max` の順です。

## Fetch Result

`Fetch Thumbnails` は、画像がない ROM に対して CRC lookup と画像取得を行います。
`Existing < Replace >` の場合は、画像がある ROM も同じファイル名へ上書きする候補として処理します。

```text
nes
reason simple_rom_crc
aliases seen 1
ROMs 50
existing 8
missing 42
CRC checked 42
CRC matched 12
downloaded 12
no match 30
CRC miss 20
image miss 10
download failed 1
invalid PNG 0
skipped zip 0
skipped tool 0
----------------
```

- `CRC checked`: CRC を計算した ROM 数です。
- `CRC matched`: libretro DAT または rescue overlay に CRC が見つかった ROM 数です。
- `downloaded`: 今回新しく保存できた PNG 数です。
- `no match`: `CRC miss` と `image miss` の合計です。
- `CRC miss`: CRC が libretro DAT と rescue overlay のどちらにも無かった ROM 数です。
- `image miss`: CRC は一致したが、canonical 名と救済候補のどちらでも libretro thumbnails に
  画像が無かった ROM 数です。
- `download failed`: URL は決まったが、通信や保存で失敗した数です。
- `invalid PNG`: ダウンロード結果が PNG として扱えなかった数です。
- `skipped zip`: ZIP を展開できない、または対象 ROM を取り出せなかった数です。
- `skipped tool`: 必要な tool、DAT、thumbnail index、source 情報が足りず処理できなかった数です。

## What To Do

- `CRC miss` が多い場合:
  正規 dump と CRC が違う、または現在の rescue overlay に未収録の可能性があります。
  吸い出し直し、別 dump の確認、rescue seed の追加、または手動で
  `/mnt/SDCARD/Images/<system>/` に画像を置く方法を使います。
- `image miss` が多い場合:
  ROM は認識されていますが、libretro 側に画像が無い、または現在の救済候補では届かない
  名前になっています。手動画像を置くのが確実です。
- `download failed` が出る場合:
  Wi-Fi、DNS、サーバ応答、SD card の書き込み状態を確認し、時間を置いて再実行します。
- `invalid PNG` が出る場合:
  サーバ応答が画像ではない可能性があります。時間を置いて再実行します。
- `skipped zip` が出る場合:
  ZIP 内に対応 ROM payload が無い、または展開できない可能性があります。
  ZIP の外側 file 名が日本語であること自体は問題ではありません。ZIP 内の通常 file が
  ROM payload 1 本だけの場合は、内部 member 名が文字化けしていても救済します。`gbc`
  配下に `.gb` payload が混入している場合は GB source で救済しますが、未知の拡張子、
  複数 payload、壊れた ZIP は skipped になります。ZIP の中身を確認し、必要なら ROM を
  正しい system directory に移します。
- `skipped tool` が出る場合:
  plumOS runtime、cache、preload data、network source のどれかが不足しています。
  plumOS を更新し、ネットワーク接続と `/mnt/SDCARD/plumos/share/frontend/artwork-scraper/`
  を確認します。

ユーザが自分で用意した画像は、スクレイピング対象外 system でも表示できます。
画像は `/mnt/SDCARD/Images/<system>/` 配下に置きます。
