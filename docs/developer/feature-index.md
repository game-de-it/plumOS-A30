# plumOS Feature Reverse Lookup

This page is a developer-oriented reverse lookup for plumOS features. Use it
when you know the feature or behavior you want to change, but not the relevant
source, config, build artifact, or verification document yet.

Japanese counterpart: [feature-index.ja.md](feature-index.ja.md)

## How To Read This Page

- `Runtime path` describes where the feature lives on the A30 SD card.
- `Source / config` points to the repository files most likely to need edits.
- `Build / deploy` names the usual build target or host-side helper.
- `Reference docs` point to deeper design notes or verification records.

When a change affects frontend behavior, rebuild `frontend`, deploy it, then use
`scripts/a30-fe-control.sh restart` and `status` before judging the screen.

## Frontend And UI

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| TOP, ROM list, START, Settings, Power, Apps UI | `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` | `src/frontend/plumos_controller_ui.c`, `src/frontend/plumos_text_ui.c`, `src/frontend/plumos_frontend.c` | `./scripts/docker-build.sh frontend`, `scripts/deploy-a30.sh`, `scripts/a30-fe-control.sh` | [Frontend Data Model](../frontend-data-model.md), [A30 UI Design](../a30-ui-design.md) |
| Mali/TTY rendering, rotation, framebuffer ownership | `/dev/fb0`, `/dev/mali` | `src/frontend/plumos_mali_renderer.h`, `src/frontend/plumos_controller_ui.c`, `src/probe/plumos_fb_restore.c` | `frontend`, `scripts/capture-a30-framebuffer.sh` | [A30 Runtime Probe](../a30-runtime-probe.md), [A30 Stock SDL Video](../a30-stock-sdl-video.md) |
| Boot splash | `/mnt/SDCARD/plumos/config/frontend/boot-splash.png` | `src/frontend/plumos_boot_splash.c`, `package/frontend/plumos/config/frontend/boot-splash.png` | `frontend` | [MainUI Bootstrap](../mainui-bootstrap.md) |
| Theme layout and artwork assets | `/mnt/SDCARD/plumos/themes/` | `package/frontend/plumos/themes/*/theme.json`, `package/frontend/plumos/themes/default/images/` | `frontend`, release package rebuild | [Frontend Theme Model](../frontend-theme-model.md), [A30 UI Design](../a30-ui-design.md) |
| Localization and translated UI labels | `/mnt/SDCARD/plumos/share/frontend/lang/*.lang` | `package/frontend/plumos/share/frontend/lang/*.lang`, UI label keys in `src/frontend/` | `frontend` | [A30 Settings Policy](../a30-settings-policy.md) |

## Systems, ROM Lists, And Launch Profiles

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| System list, ROM directories, extensions, default cores | `/mnt/SDCARD/plumos/config/frontend/systems.json` | `package/frontend/plumos/config/frontend/systems.json`, `scripts/generate-systems-json-from-libretro-map.py` | `frontend`, `scripts/audit-launch-profiles.py` | [Supported Systems](../user/emulators.md), [FE Executable Targets](../emulator-fe-targets.md) |
| Apps menu entries | `/mnt/SDCARD/plumos/config/frontend/apps.json` | `package/frontend/plumos/config/frontend/apps.json` | `frontend` | [Frontend Data Model](../frontend-data-model.md) |
| Menus and Settings hierarchy | `/mnt/SDCARD/plumos/config/frontend/menus.json` | `package/frontend/plumos/config/frontend/menus.json`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 Settings Policy](../a30-settings-policy.md) |
| Core selection, per-system and per-ROM overrides | `/mnt/SDCARD/plumos/state/frontend/core-overrides.json` | `src/frontend/plumos_controller_ui.c`, launchers under `/mnt/SDCARD/plumos/bin/` | `frontend`, emulator runtime package | [Emulator Runtime Manifest](../emulator-runtime-manifest.md), [Emulator Issue Triage](../emulator-issue-triage.md) |
| Favorites, recent items, cursor restoration | `/mnt/SDCARD/plumos/state/frontend/` | `src/frontend/plumos_library_scan.c`, `src/frontend/plumos_controller_ui.c` | `frontend` | [Frontend Data Model](../frontend-data-model.md) |
| Directory-as-game and subdirectory navigation | ROM directories under `/mnt/SDCARD/Roms/` | `src/frontend/plumos_library_scan.c` | `frontend` | [Frontend Prototype](../frontend-prototype.md) |

## Artwork Scraping

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| Thumbnail scraping command | `/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper` | `package/frontend/plumos/bin/plumos-thumbnail-scraper` | `frontend`, `scripts/benchmark-a30-thumbnail-scraper.sh` | [Artwork Scraper Prototype](../artwork-scraper-prototype.md), [Thumbnail Scraping Results](../thumbnail-scraping-results.md) |
| Scraper source DB and system enablement | `/mnt/SDCARD/plumos/config/frontend/scraper-*.tsv`, `systems.json` | `package/frontend/plumos/config/frontend/scraper-sources.tsv`, `scraper-rescue-seeds.tsv`, `systems.json` | `scripts/prefetch-thumbnail-scraper-cache.sh`, `scripts/build-thumbnail-rescue-overlays.py` | [Third-Party Data](../third-party-data.md) |
| Images output directory policy | `/mnt/SDCARD/Images/<rom_dir>/` | `plumos-thumbnail-scraper`, ROM scan config in `systems.json` | `frontend` | [User Storage](../user/storage.md), [Thumbnail Scraping Results](../thumbnail-scraping-results.md) |

## Emulators And Runtimes

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| RetroArch runtime and launcher | `/mnt/SDCARD/plumos/retroarch/`, `/mnt/SDCARD/plumos/bin/plumos-retroarch-launch` | `docker/plumos-toolchain/scripts/build-retroarch-practical.sh`, `package/frontend/plumos/factory-defaults/ra/` | `./scripts/docker-build.sh retroarch-practical` | [RetroArch Build Options](../retroarch-build-options.md), [RetroArch Baseline](../retroarch-known-working-baseline.md) |
| libretro core recipes and source locks | `/mnt/SDCARD/plumos/retroarch/cores/` | `docker/plumos-toolchain/libretro-core-recipes.tsv`, `docs/onion-libretro-source-lock.tsv` | `./scripts/docker-build.sh libretro-cores`, `scripts/update-libretro-built-core-inventory.py` | [libretro Core Version Inventory](../libretro-core-version-inventory.md), [Emulator Runtime Manifest](../emulator-runtime-manifest.md) |
| PicoArch runtime and A30 presenter/input patches | `/mnt/SDCARD/plumos/emulators/picoarch/`, `/mnt/SDCARD/plumos/bin/plumos-picoarch-launch` | `docker/plumos-toolchain/scripts/build-picoarch.sh`, `docker/plumos-toolchain/patches/picoarch-*.patch` | `./scripts/docker-build.sh picoarch` | [Emulator Issue Triage](../emulator-issue-triage.md), [A30 Input Policy](../a30-input-policy.md) |
| Standalone emulator launcher | `/mnt/SDCARD/plumos/bin/plumos-standalone-launch` | `docker/plumos-toolchain/scripts/build-standalone-emulators.sh`, `package/frontend/plumos/config/frontend/systems.json` | `./scripts/docker-build.sh standalone-emulators` | [Emulator Runtime Manifest](../emulator-runtime-manifest.md), known-good baseline docs |
| PPSSPP defaults and recovery | `/mnt/SDCARD/plumos/emulators/ppsspp/`, `/mnt/SDCARD/plumos/state/standalone/ppsspp/` | `docker/plumos-toolchain/patches/ppsspp-*.patch`, `package/frontend/plumos/factory-defaults/sa/` | `standalone-emulators`, `plumos-factory-reset` | [PPSSPP Baseline](../ppsspp-known-good-baseline.md) |
| ScummVM standalone | `/mnt/SDCARD/plumos/emulators/scummvm/`, `/mnt/SDCARD/plumos/state/standalone/scummvm/` | `docker/plumos-toolchain/patches/scummvm-*.patch`, `build-standalone-emulators.sh` | `standalone-emulators` | [ScummVM Baseline](../scummvm-known-working-baseline.md) |
| Pyxel runtime | `/mnt/SDCARD/plumos/python/`, `/mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch` | `docker/plumos-toolchain/scripts/build-python-runtime.sh`, `build-pyxel-a30.sh`, `pyxel-a30-shim/`, SDL3 `a30mali` patches | `./scripts/docker-build.sh python-runtime`, `pyxel-a30` | [Pyxel A30 Output](../pyxel-a30-output.md) |
| OpenBOR, EasyRPG, PCSX-ReARMed | `/mnt/SDCARD/plumos/emulators/<name>/` | `build-standalone-emulators.sh`, matching patches in `docker/plumos-toolchain/patches/` | `standalone-emulators` | [Emulator Runtime Manifest](../emulator-runtime-manifest.md) |

## Input, Hotkeys, Power, And Device State

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| A30 physical input and virtual gamepad | `/mnt/SDCARD/plumos/bin/plumos-joystickd` | `src/input/plumos_joystickd.c`, `src/input/plumos_joystick_reader.c` | `./scripts/docker-build.sh joystickd`, `scripts/probe-a30-joystickd-*.sh` | [A30 joystickd](../a30-joystickd.md), [A30 Input Policy](../a30-input-policy.md) |
| Safe hotkeys, emulator power menu, volume keys | `/mnt/SDCARD/plumos/bin/plumos-safe-hotkeyd`, `plumos-power-menu-overlay` | `src/input/plumos_safe_hotkeyd.c`, `package/frontend/plumos/bin/plumos-power-menu-overlay` | `joystickd`, `frontend` | [A30 Input Policy](../a30-input-policy.md), [A30 System Notes](../a30-system-notes.md) |
| Sleep, shutdown, and poweroff flows | `/mnt/SDCARD/plumos/bin/plumos-safe-shutdown` | `package/frontend/plumos/bin/plumos-safe-shutdown`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 System Notes](../a30-system-notes.md) |
| Volume and brightness runtime control | `/mnt/SDCARD/plumos/bin/plumos-volume-control`, system settings | `package/frontend/plumos/bin/plumos-volume-control`, `package/frontend/plumos/config/system/settings.json` | `frontend` | [A30 Settings Policy](../a30-settings-policy.md), [Config Layout Policy](../plumos-config-layout.md) |
| CPU frequency/core policy | `/sys/devices/system/cpu/`, FE launch environment | launchers in `/mnt/SDCARD/plumos/bin/`, `src/frontend/plumos_controller_ui.c` | `frontend`, emulator runtime builds | [A30 System Notes](../a30-system-notes.md), [A30 Settings Policy](../a30-settings-policy.md) |

## Network And External Access

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| SSH/SFTP service | `/mnt/SDCARD/plumos/ssh/`, `/mnt/SDCARD/plumos/ssh/start-ssh.sh` | `package/ssh-kit/`, `scripts/build-ssh-kit.sh` | `./scripts/build-ssh-kit.sh`, runtime package rebuild | [SSH Bring-up](../ssh-bringup.md), [Network Services](../network-services.md) |
| FTP/SFTP/Samba toggles | `/mnt/SDCARD/plumos/bin/plumos-network-services` | `package/network-services/`, `package/frontend/plumos/bin/plumos-network-control` | `./scripts/docker-build.sh network-services`, `frontend` | [Network Services](../network-services.md) |
| Wi-Fi status/recovery | `/mnt/SDCARD/plumos/bin/plumos-network-rescue` | `package/frontend/plumos/bin/plumos-network-rescue`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 System Notes](../a30-system-notes.md), [Network Services](../network-services.md) |
| USB Disk Mode | `/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode` | `package/network-services/`, `package/frontend/plumos/bin/plumos-sdcard-cleanup` | `network-services`, `frontend` | [USB Disk Mode](../usb-disk-mode.md) |

## Apps

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| File manager | `/mnt/SDCARD/plumos/apps/nextcommander/` | `docker/plumos-toolchain/scripts/build-nextcommander.sh`, `nextcommander-a30.patch`, `apps.json` | `./scripts/docker-build.sh nextcommander`, `frontend` | [Developer Guide](README.md) |
| Music player | `/mnt/SDCARD/plumos/apps/music-player/` | `src/apps/plumos_music_player.c`, `docker/plumos-toolchain/scripts/build-music-player.sh`, `apps.json` | `./scripts/docker-build.sh music-player` | [plumOS Music Player](../plumos-music-player.md) |
| RetroArch and PPSSPP standalone app entries | Apps menu | `package/frontend/plumos/config/frontend/apps.json`, launch scripts under `/mnt/SDCARD/plumos/bin/` | `frontend`, runtime package rebuild | [User Operation](../user/operation.md) |

## Boot, Packaging, And Release

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| MainUI wrapper and StockOS fallback | `/mnt/SDCARD/miyoo/app/MainUI`, `MainUI.stock` | `package/bootstrap/plumos/bootstrap/MainUI.wrapper`, `scripts/build-bootstrap-package.sh`, `scripts/install-mainui-wrapper-a30.sh` | `./scripts/build-bootstrap-package.sh`, `scripts/a30-fe-control.sh` | [MainUI Bootstrap](../mainui-bootstrap.md) |
| Runtime package | `/mnt/SDCARD/plumos/` | `scripts/build-runtime-package.py` and all `dist/plumos-*` inputs | `./scripts/build-runtime-package.py` | [Runtime Package](../runtime-package.md) |
| SD-root release package | SD card root | `scripts/build-sdroot-package.py`, `scripts/audit-release-sdroot.py` | `./scripts/build-sdroot-package.py`, `.7z` archive step | [SD Root Package](../sdroot-package.md), [Release Artifacts](../release-artifacts.md) |
| Developer package | release asset | `scripts/build-dev-package.py` | `./scripts/build-dev-package.py` | [Developer Package](../developer-package.md) |
| Release bundle | `dist/plumos-release-<version>/` | `scripts/build-release-bundle.py` | `./scripts/build-release-bundle.py --version <version>` | [Release Artifacts](../release-artifacts.md) |
| Factory reset | `/mnt/SDCARD/plumos/bin/plumos-factory-reset`, `plumos/factory-defaults/` | `package/frontend/plumos/bin/plumos-factory-reset`, `package/frontend/plumos/factory-defaults/` | `frontend`, runtime package rebuild | [Config Layout Policy](../plumos-config-layout.md), baseline docs |

## Diagnostics And Hardware Verification

| Feature / task | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| Run commands on A30 | SSH target | `scripts/run-a30.sh` | host helper | [Developer Guide](README.md) |
| Deploy files to A30 | `/mnt/SDCARD` | `scripts/deploy-a30.sh` | host helper | [Docker Build Guide](build.md) |
| Start/stop/restart FE safely | `/dev/fb0` owner | `scripts/a30-fe-control.sh` | host helper | [A30 Runtime Probe](../a30-runtime-probe.md) |
| Stop stale emulators/display owners | `/dev/fb0` owner | `scripts/stop-a30-display-processes.sh` | host helper | [A30 Runtime Probe](../a30-runtime-probe.md) |
| Capture the screen | `/dev/fb0` | `scripts/capture-a30-framebuffer.sh` | host helper | [A30 Runtime Probe](../a30-runtime-probe.md) |
| Collect logs and state | `/mnt/SDCARD/plumos/logs/` | `scripts/collect-a30-logs.sh`, `scripts/collect-a30-info.sh` | host helper | [A30 System Notes](../a30-system-notes.md) |
| Probe display/input/runtime paths | probe binaries and scripts | `src/probe/`, `scripts/probe-a30-*.sh` | relevant `docker-build.sh` target | [A30 Runtime Probe](../a30-runtime-probe.md), [A30 Stock SDL Video](../a30-stock-sdl-video.md) |

## Quick Build Target Map

| If you changed... | Usually rebuild... |
| --- | --- |
| `src/frontend/*` or `package/frontend/plumos/config/frontend/*` | `./scripts/docker-build.sh frontend` |
| `src/input/*` | `./scripts/docker-build.sh joystickd` |
| `package/network-services/*` | `./scripts/docker-build.sh network-services` |
| `docker/plumos-toolchain/patches/sdl3-*` | `./scripts/docker-build.sh sdl2-runtime` and affected apps |
| `docker/plumos-toolchain/libretro-core-recipes.tsv` | `./scripts/docker-build.sh libretro-cores` |
| `docker/plumos-toolchain/patches/picoarch-*` | `./scripts/docker-build.sh picoarch` |
| standalone emulator patches | `./scripts/docker-build.sh standalone-emulators` |
| Python/Pyxel runtime files | `./scripts/docker-build.sh python-runtime` and `pyxel-a30` |
| release/package scripts | `./scripts/build-runtime-package.py`, `./scripts/build-sdroot-package.py`, or `./scripts/build-release-bundle.py` |
