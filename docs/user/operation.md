# Basic Operation

This document describes the plumOS frontend controls. In-game controls can vary
by runtime and emulator.

## Common Buttons

| Button | Action |
| --- | --- |
| D-pad Up/Down | Move one item |
| D-pad Left/Right | Page, or change a setting value |
| A | Confirm / run |
| B | Back / cancel |
| START | Open the START menu |
| Short POWER press | Open the Power menu |
| Function | Usually reserved for emulator menus |

TOP and ROM lists move one item with Up/Down and one page with Left/Right.

## TOP Screen

The TOP screen shows enabled systems. Choose a system to open its ROM list.
Favorites and Recent are shown alongside the system list.

## ROM List

- A: launch the selected ROM
- B: return to TOP
- START: open the START menu
- Left/Right: page

When you leave a ROM list and return later, plumOS restores the last cursor
position for that system. Gallery mode follows the same rule.

## Core Settings

Runtime/core choices can be configured per system or per ROM.

| Prefix | Meaning |
| --- | --- |
| RA | RetroArch |
| PICO | PicoArch |
| SA | Standalone |
| PYXEL | Pyxel |

Resetting a ROM-level core setting to `Default` makes it inherit the system
setting again.

## START Menu

| Item | Purpose |
| --- | --- |
| UI Settings | UI mode, display behavior, TOP refresh |
| System Settings | Volume, brightness, language, device info, factory reset |
| Network Settings | Wi-Fi status and network services |
| Performance Settings | CPU frequency/core policy for emulator launches |
| Apps | File manager, music player, standalone app entries |
| HELP | Help |
| Shutdown | Shutdown path |

## UI Settings

`Refresh TOP` rescans and reloads TOP-screen system/ROM state. A short refresh
screen is displayed even when the operation is fast. Closing the START menu does
not automatically refresh TOP.

`Open previous ROM at boot` has three choices:

| Value | Behavior |
| --- | --- |
| Off | Do not open a ROM at boot |
| On | Open the previous ROM directly |
| Recent | Open the Recent screen |

## System Settings

Main items:

- Volume
- Brightness
- Lumination
- Display Color
- Language
- INFORMATION
- Factory Reset

Factory Reset restores RetroArch, PicoArch, and standalone emulator settings to
the packaged factory defaults.

## Power Menu

Open the Power menu with a short POWER press.

| Item | Action |
| --- | --- |
| Sleep | Enter sleep |
| Shutdown | Shut down |
| Cancel | Return to the previous screen |

plumOS does not automatically save in-game progress or emulator states before
sleep/shutdown. Save your game or state manually before using these actions.

## Network Settings

`NW Service` toggles:

| Service | Purpose |
| --- | --- |
| SSH | Remote shell and SFTP entry |
| FTP | Fast file transfer |
| SFTP | SSH-based file transfer |
| Samba | Network share access from Windows/macOS |

All services share `/mnt/SDCARD/`.

## Apps

Typical app entries:

- File Manager
- Music Player
- RetroArch standalone launch
- PPSSPP standalone launch

## Emulator Menus

- RetroArch: the Function button is reserved for opening the menu. SELECT-based
  hotkeys are handled by RetroArch configuration.
- PicoArch: the Function button opens the PicoArch menu. Use the D-pad in the
  PicoArch menu.
- Standalone emulators: menu behavior depends on the emulator.

The Power menu is opened by short POWER press regardless of runtime.

## Japanese Counterpart

- [Japanese operation guide](operation.ja.md)
