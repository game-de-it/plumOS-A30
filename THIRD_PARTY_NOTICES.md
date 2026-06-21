# Third-Party Notices

This file records the main third-party software, data, fonts, and StockOS-side
payloads used by plumOS A30.

plumOS-authored code, documentation, scripts, configuration, and artwork in this
repository are licensed under the MIT License unless a file says otherwise.
Third-party components remain under their own licenses. The MIT License for
plumOS does not relicense bundled emulators, libretro cores, StockOS files,
fonts, libraries, databases, ROMs, BIOS files, or user-provided content.

ROMs and BIOS files are not included in plumOS releases.

Japanese counterpart: [THIRD_PARTY_NOTICES.ja.md](THIRD_PARTY_NOTICES.ja.md)

## Release Boundary

The end-user SD-root release is an overlay for the Miyoo A30 StockOS SD-card
boot flow. It may include StockOS SD-side runtime files so that fallback,
bootstrapping, and compatibility continue to work on a fresh SD card.

StockOS-origin files are not covered by the plumOS MIT License. Before a public
binary SD-root release, confirm the redistribution policy for the StockOS SD
payload that is included in that archive.

The developer release should accompany binary releases. It contains the plumOS
source snapshot, build scripts, recipes, and patches needed to rebuild the
plumOS-authored parts and source-built third-party components.

## Source and Version Records

The source of truth for emulator/core provenance is split across these files:

| File | Purpose |
| --- | --- |
| `docs/emulator-runtime-manifest.tsv` | FE-adopted launch profiles, source repositories, refs, build options, and verification status. |
| `docker/plumos-toolchain/libretro-core-recipes.tsv` | libretro core build recipes. |
| `docs/onion-libretro-source-lock.tsv` | Onion-era libretro source provenance used by plumOS recipes. |
| `docs/libretro-core-version-inventory.tsv` | Inventory connecting Onion data, source locks, and plumOS builds. |
| `docs/third-party-data.md` | Scraper/data provenance for thumbnail and rescue-index inputs. |

When a component build copies an upstream `LICENSE`, `COPYING`, or `README`
file into `dist/`, that copied file is the authoritative license text for that
component in the binary package.

## Major Bundled Components

| Component | Upstream / origin | License notes | plumOS use |
| --- | --- | --- | --- |
| StockOS SD payload | Miyoo A30 StockOS SD-card files | Vendor/original StockOS terms; not covered by the plumOS MIT License. Redistribution policy must be confirmed before public SD-root releases. | Boot flow, fallback `MainUI.stock`, stock runtime libraries, and compatibility files such as `miyoo/`, `Emu/`, `RApp/`, `App/`, `RetroArch/`, and `Themes/`. |
| RetroArch | <https://github.com/libretro/RetroArch> | GPLv3 license text is copied as `RetroArch-COPYING` by the build. | Main RetroArch frontend runtime. |
| libretro cores | Individual repositories listed in `docs/emulator-runtime-manifest.tsv` and `docker/plumos-toolchain/libretro-core-recipes.tsv` | Each core keeps its upstream license. The core build copies available upstream `LICENSE`/`COPYING` files into the build docs. | Emulator cores used by RetroArch and PicoArch. |
| PicoArch | <https://github.com/shauninman/picoarch> | PicoArch license text is copied to `plumos/share/doc/picoarch/LICENSE`; PicoArch also carries libpicofe and individual core licenses. | Lightweight libretro frontend. |
| PPSSPP | <https://github.com/hrydgard/ppsspp> | Upstream PPSSPP license applies. | Standalone PSP emulator. |
| ScummVM | <https://github.com/scummvm/scummvm> | Upstream ScummVM license applies. | Standalone ScummVM emulator. |
| EasyRPG Player | <https://github.com/EasyRPG/Player> | Upstream EasyRPG Player license applies. | Standalone RPG Maker 2000/2003 player. |
| PCSX-ReARMed | <https://github.com/notaz/pcsx_rearmed> | Upstream PCSX-ReARMed license applies. | Standalone PlayStation emulator. |
| OpenBOR | <https://github.com/DCurrent/openbor> | BSD-style license text is copied to `plumos/docs/openbor-LICENSE`. | Standalone OpenBOR engine. |
| Pyxel | <https://github.com/kitao/pyxel> | MIT license text is bundled in the installed Pyxel package. | Python/Pyxel game runtime. |
| Python | <https://www.python.org/> | Python Software Foundation License; full text is bundled in the Python runtime. | Python runtime and `pip` bootstrap path for Pyxel and user scripts. |
| SDL3 / sdl2-compat | <https://github.com/libsdl-org/SDL>, <https://github.com/libsdl-org/sdl2-compat> | Upstream SDL license terms apply. | plumOS SDL2-compatible runtime with A30 Mali presentation support. |
| BusyBox | <https://busybox.net/> | BusyBox is GPL-licensed; version and patch details are recorded in `plumos/share/doc/busybox/manifest.txt`. | Userland applets and FTP helper. |
| GNU/Debian userland tools | Debian/source packages for `ip`, `ss`, `strace`, `rsync`, and runtime libraries | Each package keeps its upstream/Debian license. Version and dependency details are recorded in `plumos/share/doc/gnu-userland/`. | Diagnostics and file transfer helpers. |
| Dropbear | <https://matt.ucc.asn.au/dropbear/dropbear.html> | Dropbear upstream license applies. | SSH server and key tools. |
| OpenSSH SFTP server | <https://www.openssh.com/> | OpenSSH upstream license applies. | SFTP service. |
| Samba | <https://www.samba.org/> | Samba upstream license applies. | SMB file sharing. |
| dosfstools / fsck.fat | <https://github.com/dosfstools/dosfstools> | Upstream dosfstools license applies. | FAT filesystem checking before USB Disk Mode workflows. |
| FFmpeg / libav libraries | <https://ffmpeg.org/> | FFmpeg/libav license terms depend on build configuration and linked libraries. | Music player fallback decoders and attached-picture parsing. |
| miniaudio | <https://github.com/mackron/miniaudio> | Upstream miniaudio license applies. | Music player MP3/FLAC/WAV decoding path. |
| NextCommander | <https://github.com/LoveRetro/NextCommander> | No upstream `LICENSE` file was present in the inspected repository. Confirm redistribution terms or replace this component before public release. | File manager app. |
| Noto Sans JP | <https://fonts.google.com/noto/specimen/Noto+Sans+JP> | SIL Open Font License text is bundled as `NotoSansJP-OFL.txt`. | Japanese/CJK UI font coverage. |
| WenQuanYi Micro Hei | <https://wenq.org/> | Apache-2.0 text is bundled as `WQYMicroHei-Apache-2.0.txt`. | CJK fallback font coverage. |
| libretro database / thumbnails | <https://github.com/libretro/libretro-database>, <https://github.com/libretro-thumbnails> | `libretro-database` declares CC-BY-SA-4.0. See `docs/third-party-data.md`. | Thumbnail scraper matching sources. |
| No-Intro / DAT-o-MATIC and other rescue seed sources | <https://datomatic.no-intro.org/>, <https://no-intro.org/>, plus seed repositories listed in `docs/third-party-data.md` | Used only to generate reduced rescue overlays. Raw DAT sets are not redistributed wholesale by plumOS. | CRC rescue mapping for user-owned ROM libraries. |

## Distribution Checklist

Before publishing a binary SD-root release:

- keep `LICENSE`, `THIRD_PARTY_NOTICES.md`, and
  `THIRD_PARTY_NOTICES.ja.md` in the archive root
- ship the developer source/toolchain package next to the binary SD-root
  package
- keep `docs/emulator-runtime-manifest.tsv` and build recipes synchronized with
  the binaries in the release
- exclude ROMs, BIOS files, saves, states, screenshots, videos, network
  secrets, and personal SSH keys
- confirm the StockOS SD payload redistribution policy
- confirm or replace components whose upstream redistribution terms are unclear,
  currently including NextCommander
