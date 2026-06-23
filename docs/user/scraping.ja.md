# ROM サムネイルスクレイピング

plumOS は、対応システムの ROM サムネイルを libretro database と thumbnail index から取得できます。
frontend では `START -> Apps -> Scraping` から実行します。

English counterpart: [scraping.md](scraping.md)

## 何をする機能か

scraper は ROM file の CRC32 を計算し、事前生成された libretro DAT index と照合して、
一致した場合に PNG thumbnail を `Images/` 以下へ保存します。

通常の保存先は、ROM を置いた directory 名に合わせます。

```text
Roms/snes/Akumajou Densetsu.sfc
Images/snes/Akumajou Densetsu.png

Roms/SFC/Akumajou Densetsu.sfc
Images/SFC/Akumajou Densetsu.png
```

`Imgs/` は StockOS 互換または旧 artwork 用です。新しい plumOS 用 thumbnail は `Images/` を
使ってください。

## frontend からの実行

1. `START -> Apps -> Scraping` を開く。
2. 画像種別を選ぶ。
   - `Box Art`
   - `Title Screen`
3. 既存画像をスキップするか、置き換えるかを選ぶ。
4. `ALL` または対応システムを選ぶ。
5. A ボタンで scraping を開始する。
6. 完了後に結果画面を確認する。

frontend から実行する場合、最初に ROM library scan が走るため、追加した ROM は frontend を
再起動しなくても scraping 対象になります。

## 既存画像の扱い

既定では既存 thumbnail をスキップします。これはユーザーが手動で置いた画像を守るためです。

`Replace` を選んだ場合、download に成功したときだけ同じ PNG path を上書きします。
scraping は ROM、save、BIOS、emulator 設定を変更しません。

## 対応システム

この表は `plumos/config/frontend/systems.json` の `scraper.enabled=true` を反映しています。
照合元は `plumos/config/frontend/scraper-sources.tsv` です。

| system_id | 表示名 | 主な ROM directory | scraping 対象拡張子 | CRC workers | Download workers |
| --- | --- | --- | --- | --- | --- |
| `nes` | NES | `FC, nes, famicom` | `nes, unf, unif, zip` | `8` | `6` |
| `fds` | FDS | `FDS, FC, fds` | `fds, zip` | `8` | `6` |
| `sfc` | SFC | `SFC, sfc, snes` | `sfc, smc, fig, bs, swc, zip` | `8` | `6` |
| `gb` | GB | `GB, gb` | `gb, zip` | `8` | `6` |
| `gbc` | GBC | `GBC, GB, gbc` | `gbc, zip` | `8` | `6` |
| `gba` | GBA | `GBA, gba` | `gba, zip` | `4` | `4` |
| `megadrive` | Mega Drive | `MD, megadrive, genesis` | `gen, md, smd, bin, zip` | `8` | `6` |
| `mastersystem` | Master System | `MS, MD, mastersystem` | `sms, bin, zip` | `8` | `6` |
| `gamegear` | Game Gear | `GG, GameGear, MD, gamegear` | `gg, zip` | `8` | `6` |
| `sega32x` | 32X | `THIRTYTWOX, MD, sega32x` | `32x, bin, zip` | `8` | `6` |
| `pcengine` | PC Engine | `PCE, pcengine, tg16` | `pce, sgx, zip` | `8` | `6` |
| `supergrafx` | SuperGrafx | `SGX, PCE, supergrafx` | `sgx, pce, zip` | `8` | `6` |
| `ngp` | NGP | `NGP, ngp` | `ngp, zip` | `8` | `6` |
| `ngpc` | NGPC | `NGPC, NGP, ngpc` | `ngc, ngpc, zip` | `8` | `6` |
| `wonderswan` | WonderSwan | `WS, WSC, wonderswan` | `ws, zip` | `8` | `6` |
| `wonderswancolor` | WonderSwan Color | `WSC, wonderswancolor` | `wsc, zip` | `8` | `6` |
| `lynx` | Atari Lynx | `LYNX, lynx` | `lnx, lyx, zip` | `8` | `6` |
| `virtualboy` | Virtual Boy | `VB, virtualboy` | `vb, vboy, bin, zip` | `8` | `6` |
| `msx` | MSX | `MSX, msx` | `rom, mx1, mx2, zip` | `8` | `6` |
| `atari2600` | Atari 2600 | `ATARI2600, atari2600` | `a26, bin, zip` | `8` | `6` |
| `atari7800` | Atari 7800 | `ATARI7800, atari7800` | `a78, bin, zip` | `8` | `6` |
| `vectrex` | Vectrex | `VECTREX, vectrex` | `vec, bin, zip` | `8` | `6` |
| `supervision` | Supervision | `SUPERVISION, supervision` | `sv, bin, zip` | `8` | `6` |
| `odyssey2` | Odyssey2 | `ODYSSEY2, VIDEOPAC, odyssey2` | `bin, zip` | `8` | `6` |
| `pokemini` | Pokemon Mini | `POKEMINI, pokemini` | `min, zip` | `8` | `6` |
| `atari5200` | Atari 5200 | `FIFTYTWOHUNDRED, atari5200` | `a52, bin, zip` | `8` | `6` |
| `channelf` | Fairchild Channel F | `FAIRCHILD, channelf` | `bin, chf, rom, zip` | `8` | `6` |
| `colecovision` | ColecoVision | `COLECO, coleco` | `rom, ri, mx1, mx2, col, sg, sc, zip` | `8` | `6` |
| `intellivision` | Intellivision | `INTELLIVISION, intellivision` | `int, bin, rom, zip` | `8` | `6` |
| `sg1000` | SG-1000 | `SEGASGONE, sg-1000` | `sg, mv, bin, rom, sms, gg` | `8` | `6` |

## 対象外システム

scraping 対象外のシステムでも、ユーザーが `Images/<ROM directory>/` に手動で置いた thumbnail は
通常通り表示できます。対象外なのは自動 scraping だけです。

主な対象外理由:

- CD / disc image は代表 file の決定が必要。
- PC 系 disk image は形式ごとの DAT と照合方針が必要。
- Arcade / romset 系は zip 全体ではなく内部 ROM member と romset version に依存する。
- app、engine、contentless core は単一 ROM CRC で扱いにくい。
- frontend 側で無効化されている、または scraper source がまだ無い。

## SSH からの実行

同じ runner を SSH から実行できます。

```sh
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --list-systems
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --system gba
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --system gba
/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper --fetch --all
```

出力は system 単位の TSV です。ROM file 名は出力しません。

## ログと結果

frontend からの実行ログは `plumos/logs/` 以下に保存されます。実機の `Scraping Results`
画面では直近の実行結果を要約して確認できます。

`downloaded`, `no_match`, `crc_miss`, `download_failed` などの結果項目の意味は
[thumbnail scraping results](../thumbnail-scraping-results.ja.md) を参照してください。
