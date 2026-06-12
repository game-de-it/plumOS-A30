# Third-Party Data

この文書は、plumOS が scraping / artwork / emulator metadata のために参照・加工する
第三者データの出典と扱いをまとめます。

## libretro database / thumbnails

plumOS の thumbnail scraping は、標準照合元として `libretro-database` と
`libretro-thumbnails` を使います。

- Source: https://github.com/libretro/libretro-database
- Source: https://github.com/libretro-thumbnails
- License: `libretro-database` repository は `CC-BY-SA-4.0` を明示しています。
- Use in plumOS: build/prefetch 時に DAT と thumbnail directory index を TSV へ加工し、
  runtime では `/mnt/SDCARD/plumos/share/frontend/artwork-scraper/` 配下の縮小済み index を読みます。

## Rescue overlay seed data

libretro base DB に無い CRC でも、No-Intro / DAT-o-MATIC などの第三者データで同一ゲームを
確認できる場合があります。plumOS では、この情報を raw DAT の丸ごと再配布ではなく、
画像取得に必要な最小の rescue overlay として扱います。

- Source: https://datomatic.no-intro.org/
- Source: https://no-intro.org/
- Source: https://github.com/rinrin-/NesterJ_AoEX_R3/blob/master/src/nesterj/famicom.dat
- Use in plumOS: 検証済み seed row から build/prefetch 時に
  `crc -> libretro thumbnail href` の kind 別 TSV を生成します。
- Runtime path: `/mnt/SDCARD/plumos/share/frontend/artwork-scraper/rescue/<system>/<kind>.tsv`
- Seed path: `package/frontend/plumos/config/frontend/scraper-rescue-seeds.tsv`
- Optional build input: `PLUMOS_RESCUE_DAT_ROOTS` または
  `scripts/prefetch-thumbnail-scraper-cache.sh --rescue-dat-root <path>` で、
  事前取得・展開済みの外部DAT rootを指定できます。

Rescue overlay は以下の条件を満たす row だけを含めます。

- libretro base CRC index と重複しない。
- libretro thumbnail index に実在する label へ解決できる。
- 曖昧な title 類似検索ではなく、検証済み seed または生成時に一意解決できた候補である。
- runtime で外部サイトへDATを取りに行かない。

No-Intro / DAT-o-MATIC や NesterJ_AoEX_R3 由来データへのアクセスは、build/prefetch または
開発時の検証に限定します。DAT-o-MATIC などへ A30 runtime から自動アクセスしません。
A30 runtime は同梱済みの縮小TSVだけを読みます。

NES の `nesterj-famicom-dat/date-publisher` seed は、NesterJ_AoEX_R3 `famicom.dat` の CRC を
release date + publisher で libretro thumbnail label に一意解決できた row だけに限定します。
日付だけで一致するが複数候補がある row や publisher が不明な row は入れません。

## Credit

plumOS appreciates the work of the No-Intro, DAT-o-MATIC, NesterJ_AoEX_R3,
libretro-database, and libretro-thumbnails communities. Their databases and
thumbnail indexes make CRC-based artwork lookup practical for user-owned ROM
libraries.
