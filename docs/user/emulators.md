# Supported Systems and Emulator Profiles

This is the user-facing summary generated from the current `systems.json` and `docs/emulator-runtime-manifest.tsv`.
For exact build commits, source repositories, and verification status, see the developer-facing
[emulator runtime manifest](../emulator-runtime-manifest.md).

Japanese counterpart: [emulators.ja.md](emulators.ja.md)

## Runtime Labels

| Label | Meaning |
| --- | --- |
| RA | RetroArch |
| PICO | PicoArch |
| SA | Standalone emulator |
| PYXEL | Pyxel runtime |

`pass_init` and `untested` are retained launch candidates, but they still need additional ROM, BIOS, or gameplay confirmation.
Normal practical profiles are marked `pass`.

## ROM Directory and Default Profile

| system_id | Display name | Main recognized directories | Main extensions | default |
| --- | --- | --- | --- | --- |
| `nes` | NES | `FC`, `nes`, `famicom` | `nes`, `unf`, `unif`, `zip`, `7z` | `retroarch:fceumm` |
| `fds` | FDS | `FDS`, `FC`, `fds` | `fds`, `zip`, `7z` | `retroarch:fceumm` |
| `sfc` | SFC | `SFC`, `sfc`, `snes` | `sfc`, `smc`, `fig`, `bs`, `swc`, `zip`, `7z` | `retroarch:snes9x2005` |
| `gb` | GB | `GB`, `gb` | `gb`, `zip`, `7z` | `retroarch:gambatte` |
| `gbc` | GBC | `GBC`, `GB`, `gbc` | `gbc`, `zip`, `7z` | `retroarch:gambatte` |
| `gba` | GBA | `GBA`, `gba` | `gba`, `zip`, `7z` | `retroarch:mgba` |
| `megadrive` | Mega Drive | `MD`, `megadrive`, `genesis` | `gen`, `md`, `smd`, `bin`, `zip`, `7z` | `retroarch:genesis_plus_gx` |
| `mastersystem` | Master System | `MS`, `MD`, `mastersystem` | `sms`, `bin`, `zip`, `7z` | `retroarch:genesis_plus_gx` |
| `gamegear` | Game Gear | `GG`, `GameGear`, `MD`, `gamegear` | `gg`, `zip`, `7z` | `retroarch:genesis_plus_gx` |
| `sega32x` | 32X | `THIRTYTWOX`, `MD`, `sega32x` | `32x`, `bin`, `zip`, `7z` | `retroarch:picodrive` |
| `segacd` | Sega CD | `SEGACD`, `segacd`, `megacd` | `cue`, `chd`, `iso`, `m3u`, `bin` | `retroarch:genesis_plus_gx` |
| `pcengine` | PC Engine | `PCE`, `pcengine`, `tg16` | `pce`, `sgx`, `zip`, `7z` | `retroarch:mednafen_pce_fast` |
| `supergrafx` | SuperGrafx | `SGX`, `PCE`, `supergrafx` | `sgx`, `pce`, `zip`, `7z` | `retroarch:mednafen_supergrafx` |
| `pcenginecd` | PC Engine CD | `PCECD`, `pcenginecd`, `tg16cd` | `cue`, `chd`, `iso`, `m3u` | `retroarch:mednafen_pce_fast` |
| `psx` | PlayStation | `PS`, `PSX`, `psx` | `cue`, `chd`, `pbp`, `m3u`, `iso`, `img`, `bin` | `standalone:pcsx_rearmed` |
| `psp` | PSP | `PSP`, `PPSSPP`, `psp` | `iso`, `cso`, `pbp` | `standalone:ppsspp` |
| `neogeo` | Neo Geo | `NEOGEO`, `neogeo` | `zip`, `7z` | `retroarch:fbneo` |
| `neogeocd` | Neo Geo CD | `NEOCD`, `neogeocd` | `cue`, `chd` | `retroarch:neocd` |
| `ngp` | NGP | `NGP`, `ngp` | `ngp`, `zip`, `7z` | `retroarch:mednafen_ngp` |
| `ngpc` | NGPC | `NGPC`, `NGP`, `ngpc` | `ngc`, `ngpc`, `zip`, `7z` | `retroarch:mednafen_ngp` |
| `wonderswan` | WonderSwan | `WS`, `WSC`, `wonderswan` | `ws`, `zip`, `7z` | `retroarch:mednafen_wswan` |
| `wonderswancolor` | WonderSwan Color | `WSC`, `wonderswancolor` | `wsc`, `zip`, `7z` | `retroarch:mednafen_wswan` |
| `lynx` | Atari Lynx | `LYNX`, `lynx` | `lnx`, `lyx`, `bll`, `o`, `zip`, `7z` | `retroarch:mednafen_lynx` |
| `virtualboy` | Virtual Boy | `VB`, `virtualboy` | `vb`, `vboy`, `bin`, `zip`, `7z` | `retroarch:mednafen_vb` |
| `arcade` | Arcade | `ARCADE`, `arcade` | `zip`, `7z` | `retroarch:mame2003_plus` |
| `fbneo` | FBNeo | `FBNEO`, `ARCADE`, `fbneo` | `zip`, `7z` | `retroarch:fbneo` |
| `cps1` | CPS1 | `CPS1`, `ARCADE`, `cps1` | `zip`, `7z` | `retroarch:fbneo` |
| `cps2` | CPS2 | `CPS2`, `ARCADE`, `cps2` | `zip`, `7z` | `retroarch:fbneo` |
| `cps3` | CPS3 | `CPS3`, `ARCADE`, `cps3` | `zip`, `7z` | `retroarch:fbneo` |
| `dos` | DOS | `DOS`, `pc` | `dosz`, `conf`, `exe`, `bat`, `zip` | `retroarch:dosbox_pure` |
| `easyrpg` | EasyRPG | `EASYRPG`, `easyrpg` | `easyrpg`, `ldb`, `zip` | `standalone:easyrpg` |
| `pico8` | PICO-8 | `PICO8`, `PICO`, `pico-8` | `p8`, `png` | `picoarch:fake08` |
| `scummvm` | ScummVM | `SCUMMVM`, `scummvm` | `scummvm`, `svm` | `standalone:scummvm` |
| `openbor` | OpenBOR | `OPENBOR`, `openbor` | `pak` | `standalone:openbor` |
| `msx` | MSX | `MSX`, `msx` | `rom`, `mx1`, `mx2`, `dsk`, `cas`, `zip`, `7z` | `retroarch:bluemsx` |
| `pc88` | PC-88 | `PC88`, `pc-8800` | `d88`, `u88`, `m3u`, `zip` | `retroarch:quasi88` |
| `pc98` | PC-98 | `PC98`, `pc-9800` | `d88`, `fdi`, `hdi`, `fdd`, `nhd`, `hdd`, `m3u`, `zip` | `retroarch:np2kai` |
| `x68000` | X68000 | `X68000`, `x68000` | `dim`, `hdf`, `xdf`, `m3u`, `zip`, `7z` | `retroarch:px68k` |
| `tic80` | TIC-80 | `TIC`, `tic-80` | `tic` | `retroarch:tic80` |
| `atari2600` | Atari 2600 | `ATARI2600`, `atari2600` | `a26`, `bin`, `zip`, `7z` | `retroarch:stella2014` |
| `atari7800` | Atari 7800 | `ATARI7800`, `atari7800` | `a78`, `bin`, `cdf`, `zip`, `7z` | `retroarch:prosystem` |
| `vectrex` | Vectrex | `VECTREX`, `vectrex` | `vec`, `bin`, `zip`, `7z` | `retroarch:vecx` |
| `supervision` | Supervision | `SUPERVISION`, `supervision` | `sv`, `bin`, `zip`, `7z` | `retroarch:potator` |
| `odyssey2` | Odyssey2 | `ODYSSEY2`, `VIDEOPAC`, `odyssey2` | `bin`, `zip`, `7z` | `retroarch:o2em` |
| `gameandwatch` | Game & Watch | `GW`, `GAMEANDWATCH`, `gameandwatch` | `mgw`, `zip`, `7z` | `retroarch:gw` |
| `pokemini` | Pokemon Mini | `POKEMINI`, `pokemini` | `min`, `zip`, `7z` | `retroarch:pokemini` |
| `doom` | Doom | `DOOM`, `doom` | `wad`, `iwad`, `pwad`, `zip` | `retroarch:prboom` |
| `pyxel` | Pyxel | `pyxel`, `PYXEL` | `pyxapp`, `py` | `pyxel:a30` |
| `amiga` | Amiga | `AMIGA`, `amiga` | `adf`, `adz`, `dms`, `ipf`, `hdf`, `lha`, `cue`, `iso`, `chd`, `m3u`, `zip`, `7z` | `retroarch:puae` |
| `atari5200` | Atari 5200 | `FIFTYTWOHUNDRED`, `atari5200` | `a52`, `bin`, `zip`, `7z` | `retroarch:atari800` |
| `atari800` | Atari 8-bit | `EIGHTHUNDRED`, `atari800` | `atr`, `xfd`, `dcm`, `cas`, `bin`, `car`, `rom`, `xex`, `m3u`, `zip`, `7z` | `retroarch:atari800` |
| `atarist` | Atari ST | `ATARIST`, `atarist` | `st`, `msa`, `zip`, `stx`, `dim`, `ipf`, `vhd`, `gem`, `ide`, `m3u` | `retroarch:hatari` |
| `c64` | Commodore 64 | `COMMODORE`, `c64` | `d64`, `d71`, `d81`, `g64`, `x64`, `t64`, `tap`, `prg`, `crt`, `bin`, `zip`, `gz`, `m3u` | `retroarch:vice_x64` |
| `cannonball` | Cannonball | `CANNONBALL`, `cannonball` | `game`, `88` | `retroarch:cannonball` |
| `chailove` | ChaiLove | `CHAILOVE`, `chailove` | `chai`, `chailove` | `retroarch:chailove` |
| `channelf` | Fairchild Channel F | `FAIRCHILD`, `channelf` | `bin`, `chf`, `rom`, `zip` | `retroarch:freechaf` |
| `colecovision` | ColecoVision | `COLECO`, `coleco` | `rom`, `ri`, `mx1`, `mx2`, `col`, `dsk`, `cas`, `sg`, `sc`, `m3u`, `zip`, `7z` | `retroarch:bluemsx` |
| `cpc` | Amstrad CPC | `CPC`, `amstradcpc` | `dsk`, `sna`, `zip`, `tap`, `cdt`, `voc`, `cpr`, `m3u`, `kcr` | `retroarch:crocods` |
| `dinothawr` | Dinothawr | `DINOTHAWR`, `dinothawr` | `game` | `retroarch:dinothawr` |
| `intellivision` | Intellivision | `INTELLIVISION`, `intellivision` | `int`, `bin`, `rom`, `zip`, `7z` | `retroarch:freeintv` |
| `j2me` | Java ME | `J2ME`, `j2me` | `jar`, `sqc`, `jam`, `jad`, `kjx` | `retroarch:squirreljme` |
| `lowresnx` | LowRes NX | `LOWRESNX`, `lowresnx` | `nx` | `retroarch:lowresnx` |
| `lutro` | Lutro | `LUTRO`, `lutro` | `lutro`, `love`, `lua` | `retroarch:lutro` |
| `music` | Game Music Emu | `MUSIC`, `music` | `ay`, `gbs`, `gym`, `hes`, `kss`, `nsf`, `nsfe`, `sap`, `spc`, `vgm`, `vgz`, `zip` | `retroarch:gme` |
| `quake` | Quake | `QUAKE`, `quake` | `pak` | `retroarch:tyrquake` |
| `sg1000` | SG-1000 | `SEGASGONE`, `sg-1000` | `sg`, `mv`, `bin`, `rom`, `sms`, `gg` | `retroarch:gearsystem` |
| `sharpx1` | Sharp X1 | `XONE`, `x1` | `dx1`, `zip`, `2d`, `2hd`, `tfd`, `d88`, `88d`, `hdf`, `xdf`, `tap`, `cmd` | `retroarch:x1` |
| `thomson` | Thomson MO/TO | `THOMSON`, `moto` | `fd`, `sap`, `k7`, `m7`, `m5`, `rom` | `retroarch:theodore` |
| `ti83` | TI-83 | `TI83`, `ti83` | `8xp`, `8xk`, `8xg` | `retroarch:numero` |
| `vic20` | Commodore VIC-20 | `VIC20`, `vic20` | `d64`, `d71`, `d81`, `g64`, `x64`, `t64`, `tap`, `prg`, `crt`, `bin`, `cmd`, `m3u`, `zip`, `7z`, `gz`, `20`, `40`, `60`, `a0`, `b0`, `rom` | `retroarch:vice_xvic` |
| `vmu` | Dreamcast VMU | `VMU`, `vmu` | `vms`, `dci`, `bin` | `picoarch:vemulator` |
| `wolf3d` | Wolfenstein 3D | `WOLF3D`, `wolf3d` | `wl6`, `n3d`, `sod`, `sdm`, `wl1`, `pk3`, `exe` | `retroarch:ecwolf` |
| `zx81` | ZX-81 | `ZXEIGHTYONE`, `zx81` | `p`, `tzx`, `t81`, `zip` | `retroarch:81` |
| `zxspectrum` | ZX Spectrum | `ZXS`, `zxspectrum` | `tzx`, `tap`, `z80`, `rzx`, `scl`, `trd`, `dsk`, `sna`, `szx`, `zip`, `gz` | `retroarch:fuse` |

## Runtime/Core Candidates

| system_id | System | Runtime | Status | Profiles |
| --- | --- | --- | --- | --- |
| `amiga` | Amiga | RA, PICO | pass | `picoarch:puae`, `retroarch:puae` |
| `arcade` | Arcade | RA | pass | `retroarch:fbalpha2012`, `retroarch:fbneo`, `retroarch:mame2000`, `retroarch:mame2003_plus` |
| `atari2600` | Atari 2600 | PICO, RA | pass | `picoarch:stella2014`, `retroarch:stella2014` |
| `atari5200` | Atari 5200 | RA, PICO | pass_init | `picoarch:atari800`, `retroarch:atari800` |
| `atari7800` | Atari 7800 | RA, PICO | pass | `picoarch:prosystem`, `retroarch:prosystem` |
| `atari800` | Atari 8-bit | RA, PICO | pass_init | `picoarch:atari800`, `retroarch:atari800` |
| `atarist` | Atari ST | PICO, RA | pass, pass_init | `picoarch:hatari`, `retroarch:hatari` |
| `c64` | Commodore 64 | PICO, RA | pass_init | `picoarch:frodo`, `picoarch:vice_x64`, `retroarch:frodo`, `retroarch:vice_x64` |
| `cannonball` | Cannonball | PICO, RA | pass | `picoarch:cannonball`, `retroarch:cannonball` |
| `chailove` | ChaiLove | PICO, RA | pass_init | `picoarch:chailove`, `retroarch:chailove` |
| `channelf` | Fairchild Channel F | PICO, RA | untested | `picoarch:freechaf`, `retroarch:freechaf` |
| `colecovision` | ColecoVision | PICO, RA | pass | `picoarch:bluemsx`, `retroarch:bluemsx` |
| `cpc` | Amstrad CPC | PICO, RA | pass | `picoarch:cap32`, `picoarch:crocods`, `retroarch:cap32`, `retroarch:crocods` |
| `cps1` | CPS1 | PICO, RA | pass | `picoarch:fbalpha2012`, `picoarch:fbalpha2012_cps1`, `picoarch:fbneo`, `retroarch:fbalpha2012`, `retroarch:fbalpha2012_cps1`, `retroarch:fbneo` |
| `cps2` | CPS2 | RA, PICO | pass | `picoarch:fbalpha2012`, `picoarch:fbalpha2012_cps2`, `picoarch:fbneo`, `retroarch:fbalpha2012`, `retroarch:fbalpha2012_cps2`, `retroarch:fbneo` |
| `cps3` | CPS3 | RA, PICO | pass | `picoarch:fbalpha2012`, `picoarch:fbneo`, `retroarch:fbalpha2012`, `retroarch:fbneo` |
| `dinothawr` | Dinothawr | PICO, RA | pass | `picoarch:dinothawr`, `retroarch:dinothawr` |
| `doom` | Doom | RA, PICO | pass_init, pass | `picoarch:prboom`, `retroarch:prboom` |
| `dos` | DOS | PICO, RA | pass, pass_init | `picoarch:dosbox_pure`, `retroarch:dosbox_pure` |
| `easyrpg` | EasyRPG | SA | pass | `standalone:easyrpg` |
| `fbneo` | FBNeo | PICO, RA | pass | `picoarch:fbneo`, `retroarch:fbneo` |
| `fds` | FDS | PICO, RA | pass_init, pass | `picoarch:fceumm`, `picoarch:nestopia`, `retroarch:fceumm`, `retroarch:nestopia` |
| `gameandwatch` | Game & Watch | PICO, RA | pass | `picoarch:gw`, `retroarch:gw` |
| `gamegear` | Game Gear | RA, PICO | pass | `picoarch:gearsystem`, `picoarch:genesis_plus_gx`, `picoarch:picodrive`, `retroarch:gearsystem`, `retroarch:genesis_plus_gx`, `retroarch:picodrive` |
| `gb` | GB | RA, PICO | pass, pass_init | `picoarch:gambatte`, `picoarch:gearboy`, `picoarch:mgba`, `picoarch:vbam`, `retroarch:gambatte`, `retroarch:gearboy`, `retroarch:mgba`, `retroarch:tgbdual`, `retroarch:vbam` |
| `gba` | GBA | RA, PICO | pass_init, pass | `picoarch:gpsp`, `picoarch:mgba`, `retroarch:gpsp`, `retroarch:mgba` |
| `gbc` | GBC | PICO, RA | untested, pass_init | `picoarch:gambatte`, `picoarch:gearboy`, `picoarch:mgba`, `picoarch:vbam`, `retroarch:gambatte`, `retroarch:gearboy`, `retroarch:mgba`, `retroarch:tgbdual`, `retroarch:vbam` |
| `intellivision` | Intellivision | RA | pass_init | `retroarch:freeintv` |
| `j2me` | Java ME | RA | pass | `retroarch:squirreljme` |
| `lowresnx` | LowRes NX | RA, PICO | pass_init, pass | `picoarch:lowresnx`, `retroarch:lowresnx` |
| `lutro` | Lutro | RA, PICO | pass_init | `picoarch:lutro`, `retroarch:lutro` |
| `lynx` | Atari Lynx | PICO, RA | pass_init | `picoarch:handy`, `picoarch:mednafen_lynx`, `retroarch:handy`, `retroarch:mednafen_lynx` |
| `mastersystem` | Master System | RA, PICO | pass | `picoarch:gearsystem`, `picoarch:genesis_plus_gx`, `picoarch:picodrive`, `retroarch:gearsystem`, `retroarch:genesis_plus_gx`, `retroarch:picodrive` |
| `megadrive` | Mega Drive | RA, PICO | pass | `picoarch:genesis_plus_gx`, `picoarch:picodrive`, `retroarch:genesis_plus_gx`, `retroarch:picodrive` |
| `msx` | MSX | RA, PICO | pass | `picoarch:bluemsx`, `picoarch:fmsx`, `retroarch:bluemsx`, `retroarch:fmsx` |
| `music` | Game Music Emu | RA, PICO | pass | `picoarch:gme`, `retroarch:gme` |
| `neogeo` | Neo Geo | RA, PICO | pass | `picoarch:fbalpha2012`, `picoarch:fbalpha2012_neogeo`, `picoarch:fbneo`, `retroarch:fbalpha2012`, `retroarch:fbalpha2012_neogeo`, `retroarch:fbneo` |
| `neogeocd` | Neo Geo CD | PICO, RA | pass | `picoarch:neocd`, `retroarch:neocd` |
| `nes` | NES | RA, PICO | pass, pass_init | `picoarch:fceumm`, `picoarch:nestopia`, `picoarch:quicknes`, `retroarch:fceumm`, `retroarch:nestopia`, `retroarch:quicknes` |
| `ngp` | NGP | RA, PICO | pass | `picoarch:mednafen_ngp`, `retroarch:mednafen_ngp` |
| `ngpc` | NGPC | PICO, RA | pass | `picoarch:mednafen_ngp`, `retroarch:mednafen_ngp` |
| `odyssey2` | Odyssey2 | RA, PICO | pass | `picoarch:o2em`, `retroarch:o2em` |
| `openbor` | OpenBOR | SA | pass | `standalone:openbor` |
| `pc88` | PC-88 | RA | pass | `retroarch:quasi88` |
| `pc98` | PC-98 | RA | pass | `retroarch:np2kai` |
| `pcengine` | PC Engine | PICO, RA | pass | `picoarch:mednafen_pce_fast`, `picoarch:mednafen_supergrafx`, `retroarch:mednafen_pce_fast`, `retroarch:mednafen_supergrafx` |
| `pcenginecd` | PC Engine CD | PICO, RA | pass | `picoarch:mednafen_pce_fast`, `retroarch:mednafen_pce_fast` |
| `pico8` | PICO-8 | RA, PICO | pass, pass_init | `picoarch:fake08`, `retroarch:fake08` |
| `pokemini` | Pokemon Mini | PICO, RA | pass | `picoarch:pokemini`, `retroarch:pokemini` |
| `psp` | PSP | SA | pass | `standalone:ppsspp` |
| `psx` | PlayStation | RA, SA, PICO | pass | `picoarch:pcsx_rearmed`, `retroarch:pcsx_rearmed`, `standalone:pcsx_rearmed` |
| `pyxel` | Pyxel | PYXEL | pass | `pyxel:a30` |
| `quake` | Quake | RA, PICO | pass_init, pass | `picoarch:tyrquake`, `retroarch:tyrquake` |
| `scummvm` | ScummVM | SA, PICO | pass | `picoarch:scummvm`, `standalone:scummvm` |
| `sega32x` | 32X | RA, PICO | pass | `picoarch:picodrive`, `retroarch:picodrive` |
| `segacd` | Sega CD | RA, PICO | pass | `picoarch:genesis_plus_gx`, `picoarch:picodrive`, `retroarch:genesis_plus_gx`, `retroarch:picodrive` |
| `sfc` | SFC | RA, PICO | pass | `picoarch:chimerasnes`, `picoarch:mednafen_supafaust`, `picoarch:snes9x`, `picoarch:snes9x2002`, `picoarch:snes9x2005`, `picoarch:snes9x2005_plus`, `picoarch:snes9x2010`, `retroarch:chimerasnes`, `retroarch:mednafen_supafaust`, `retroarch:snes9x`, `retroarch:snes9x2002`, `retroarch:snes9x2005`, `retroarch:snes9x2005_plus`, `retroarch:snes9x2010` |
| `sg1000` | SG-1000 | PICO, RA | pass | `picoarch:bluemsx`, `picoarch:gearsystem`, `picoarch:genesis_plus_gx`, `picoarch:picodrive`, `retroarch:bluemsx`, `retroarch:gearsystem`, `retroarch:genesis_plus_gx`, `retroarch:picodrive` |
| `sharpx1` | Sharp X1 | RA, PICO | pass_init | `picoarch:x1`, `retroarch:x1` |
| `supergrafx` | SuperGrafx | PICO, RA | pass | `picoarch:mednafen_pce_fast`, `picoarch:mednafen_supergrafx`, `retroarch:mednafen_pce_fast`, `retroarch:mednafen_supergrafx` |
| `supervision` | Supervision | PICO, RA | pass | `picoarch:potator`, `retroarch:potator` |
| `thomson` | Thomson MO/TO | PICO, RA | pass | `picoarch:theodore`, `retroarch:theodore` |
| `ti83` | TI-83 | PICO, RA | untested | `picoarch:numero`, `retroarch:numero` |
| `tic80` | TIC-80 | PICO, RA | pass_init | `picoarch:tic80`, `retroarch:tic80` |
| `vectrex` | Vectrex | PICO, RA | pass | `picoarch:vecx`, `retroarch:vecx` |
| `vic20` | Commodore VIC-20 | RA, PICO | pass | `picoarch:vice_xvic`, `retroarch:vice_xvic` |
| `virtualboy` | Virtual Boy | PICO, RA | pass | `picoarch:mednafen_vb`, `retroarch:mednafen_vb` |
| `vmu` | Dreamcast VMU | PICO | pass | `picoarch:vemulator` |
| `wolf3d` | Wolfenstein 3D | PICO, RA | pass_init | `picoarch:ecwolf`, `retroarch:ecwolf` |
| `wonderswan` | WonderSwan | PICO, RA | pass | `picoarch:mednafen_wswan`, `retroarch:mednafen_wswan` |
| `wonderswancolor` | WonderSwan Color | RA, PICO | pass | `picoarch:mednafen_wswan`, `retroarch:mednafen_wswan` |
| `x68000` | X68000 | RA, PICO | pass_init | `picoarch:px68k`, `retroarch:px68k` |
| `zx81` | ZX-81 | PICO, RA | pass_init | `picoarch:81`, `retroarch:81` |
| `zxspectrum` | ZX Spectrum | PICO, RA | pass | `picoarch:fuse`, `retroarch:fuse` |

## Systems Hidden from Normal Display

The following systems are hidden from normal candidates because of performance, practicality, better alternative routes, BIOS/ROM requirements, or similar constraints.

```text
N64, Dreamcast, Saturn, MAME 2003+, 2048, 3DO, Elektronika BK,
Cave Story, Daphne, Flashback, Atari Jaguar, MicroW8, Mr.Boom,
Palm OS, PC-FX, Rick Dangerous, Uzebox
```
