# Third-Party Notices

この文書は、plumOS A30 が利用・同梱する主な第三者ソフトウェア、データ、フォント、
StockOS 側 payload の出典と扱いをまとめます。

この repository 内で plumOS 用に作成した code、document、script、config、artwork は、
ファイル内で別途明記しない限り MIT License で扱います。第三者 component はそれぞれの
license が適用されます。plumOS の MIT License は、同梱 emulator、libretro core、
StockOS file、font、library、database、ROM、BIOS、ユーザーが用意した content を
再 license しません。

plumOS release は ROM と BIOS file を含みません。

English counterpart: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Release Boundary

ユーザー向け SD-root release は、Miyoo A30 StockOS の SD card boot flow に重ねて使う
overlay です。fresh SD card でも fallback、bootstrap、互換性を維持するため、StockOS の
SD 側 runtime file を含む場合があります。

StockOS 由来 file は plumOS の MIT License の対象外です。StockOS SD payload を含む
binary SD-root release を公開する前に、その payload の再配布方針を確認してください。

binary release には developer release を併せて公開してください。developer release には、
plumOS 本体と source build した第三者 component を再 build するための source snapshot、
build script、recipe、patch を含めます。

## Source and Version Records

emulator/core の出典と build 情報は以下を正本にします。

| File | 役割 |
| --- | --- |
| `docs/emulator-runtime-manifest.tsv` | FE 採用 launch profile、source repository、ref、build option、検証状態。 |
| `docker/plumos-toolchain/libretro-core-recipes.tsv` | libretro core build recipe。 |
| `docs/onion-libretro-source-lock.tsv` | plumOS recipe が参照する Onion 時期の libretro source provenance。 |
| `docs/libretro-core-version-inventory.tsv` | Onion data、source lock、plumOS build の対応表。 |
| `docs/third-party-data.ja.md` | thumbnail/rescue index 用 scraper data の出典。 |

component build が upstream の `LICENSE`、`COPYING`、`README` を `dist/` へコピーしている場合、
binary package 内ではそのコピーを component ごとの正本 license text として扱います。

## Major Bundled Components

| Component | Upstream / origin | License notes | plumOS での用途 |
| --- | --- | --- | --- |
| StockOS SD payload | Miyoo A30 StockOS SD card files | vendor/original StockOS の条件に従います。plumOS MIT License の対象外です。公開 SD-root release 前に再配布方針を確認してください。 | boot flow、fallback `MainUI.stock`、stock runtime library、`miyoo/`、`Emu/`、`RApp/`、`App/`、`RetroArch/`、`Themes/` などの互換 file。 |
| RetroArch | <https://github.com/libretro/RetroArch> | build 時に GPLv3 license text を `RetroArch-COPYING` としてコピーします。 | メインの RetroArch frontend runtime。 |
| libretro cores | `docs/emulator-runtime-manifest.tsv` と `docker/plumos-toolchain/libretro-core-recipes.tsv` にある各 repository | 各 core の upstream license が適用されます。build で取得できた upstream `LICENSE`/`COPYING` は build docs にコピーします。 | RetroArch/PicoArch で使う emulator core。 |
| PicoArch | <https://github.com/shauninman/picoarch> | license text は `plumos/share/doc/picoarch/LICENSE` にコピーします。PicoArch は libpicofe と各 core の license も含みます。 | 軽量 libretro frontend。 |
| PPSSPP | <https://github.com/hrydgard/ppsspp> | upstream PPSSPP license が適用されます。 | standalone PSP emulator。 |
| ScummVM | <https://github.com/scummvm/scummvm> | upstream ScummVM license が適用されます。 | standalone ScummVM emulator。 |
| EasyRPG Player | <https://github.com/EasyRPG/Player> | upstream EasyRPG Player license が適用されます。 | standalone RPG Maker 2000/2003 player。 |
| PCSX-ReARMed | <https://github.com/notaz/pcsx_rearmed> | upstream PCSX-ReARMed license が適用されます。 | standalone PlayStation emulator。 |
| OpenBOR | <https://github.com/DCurrent/openbor> | BSD-style license text を `plumos/docs/openbor-LICENSE` にコピーします。 | standalone OpenBOR engine。 |
| Pyxel | <https://github.com/kitao/pyxel> | installed Pyxel package 内に MIT license text を含めます。 | Python/Pyxel game runtime。 |
| Python | <https://www.python.org/> | Python Software Foundation License。Python runtime 内に全文を含めます。 | Python runtime と Pyxel/user script 用 `pip` path。 |
| SDL3 / sdl2-compat | <https://github.com/libsdl-org/SDL>, <https://github.com/libsdl-org/sdl2-compat> | upstream SDL license が適用されます。 | A30 Mali 表示経路を持つ SDL2-compatible runtime。 |
| BusyBox | <https://busybox.net/> | GPL 系 license。version/patch は `plumos/share/doc/busybox/manifest.txt` に記録します。 | userland applet と FTP helper。 |
| GNU/Debian userland tools | `ip`, `ss`, `strace`, `rsync` と runtime library の Debian/source package | 各 package の upstream/Debian license が適用されます。version/dependency は `plumos/share/doc/gnu-userland/` に記録します。 | 診断と転送 helper。 |
| Dropbear | <https://matt.ucc.asn.au/dropbear/dropbear.html> | upstream Dropbear license が適用されます。 | SSH server/key tool。 |
| OpenSSH SFTP server | <https://www.openssh.com/> | upstream OpenSSH license が適用されます。 | SFTP service。 |
| Samba | <https://www.samba.org/> | upstream Samba license が適用されます。 | SMB file sharing。 |
| dosfstools / fsck.fat | <https://github.com/dosfstools/dosfstools> | upstream dosfstools license が適用されます。 | USB Disk Mode 前の FAT filesystem check。 |
| FFmpeg / libav libraries | <https://ffmpeg.org/> | FFmpeg/libav の license 条件は build configuration と linked library に依存します。 | 音楽プレイヤーの fallback decoder と attached picture 解析。 |
| miniaudio | <https://github.com/mackron/miniaudio> | upstream miniaudio license が適用されます。 | 音楽プレイヤーの MP3/FLAC/WAV decode path。 |
| NextCommander | <https://github.com/LoveRetro/NextCommander> | 調査した upstream repository に `LICENSE` file がありませんでした。公開配布前に再配布条件を確認するか、component を差し替えてください。 | ファイルマネージャー app。 |
| Noto Sans JP | <https://fonts.google.com/noto/specimen/Noto+Sans+JP> | SIL Open Font License text を `NotoSansJP-OFL.txt` として同梱します。 | 日本語/CJK UI font。 |
| WenQuanYi Micro Hei | <https://wenq.org/> | Apache-2.0 text を `WQYMicroHei-Apache-2.0.txt` として同梱します。 | CJK fallback font。 |
| libretro database / thumbnails | <https://github.com/libretro/libretro-database>, <https://github.com/libretro-thumbnails> | `libretro-database` は CC-BY-SA-4.0 を明示しています。詳細は `docs/third-party-data.ja.md`。 | thumbnail scraper の照合元。 |
| No-Intro / DAT-o-MATIC などの rescue seed | <https://datomatic.no-intro.org/>, <https://no-intro.org/> ほか `docs/third-party-data.ja.md` の seed repository | 縮小済み rescue overlay 生成にのみ使います。raw DAT を丸ごと再配布しません。 | ユーザー所有 ROM library の CRC rescue mapping。 |

## Distribution Checklist

binary SD-root release を公開する前に以下を確認します。

- archive root に `LICENSE`、`THIRD_PARTY_NOTICES.md`、
  `THIRD_PARTY_NOTICES.ja.md` を含める。
- binary SD-root package と一緒に developer source/toolchain package を公開する。
- release に含める binary と `docs/emulator-runtime-manifest.tsv`、build recipe を同期する。
- ROM、BIOS、save、state、screenshot、video、network secret、個人 SSH key を含めない。
- StockOS SD payload の再配布方針を確認する。
- upstream の再配布条件が不明な component を確認または差し替える。現時点では
  NextCommander が該当します。
