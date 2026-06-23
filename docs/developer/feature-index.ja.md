# plumOS 機能逆引き

このページは、plumOS 開発者向けの機能逆引きです。変更したい機能や挙動は分かっているが、
該当する source、config、build artifact、検証文書がまだ分からない時に使います。

英語版: [feature-index.md](feature-index.md)

## 読み方

- `Runtime path` は A30 の SD カード上でその機能が動く場所です。
- `Source / config` は編集候補になりやすい repository 側のファイルです。
- `Build / deploy` は通常使う build target や host 側 helper です。
- `Reference docs` は詳細な設計メモや検証記録です。

frontend を変更した場合は、`frontend` を rebuild/deploy し、
`scripts/a30-fe-control.sh restart` と `status` で `/dev/fb0` owner を確認してから
画面判断します。

## Frontend と UI

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| TOP、ROM list、START、Settings、Power、Apps UI | `/mnt/SDCARD/plumos/bin/plumos-controller-ui-mali` | `src/frontend/plumos_controller_ui.c`, `src/frontend/plumos_text_ui.c`, `src/frontend/plumos_frontend.c` | `./scripts/docker-build.sh frontend`, `scripts/deploy-a30.sh`, `scripts/a30-fe-control.sh` | [frontend data model](../frontend-data-model.ja.md), [A30 UI design](../a30-ui-design.ja.md) |
| Mali/TTY 描画、回転、framebuffer owner | `/dev/fb0`, `/dev/mali` | `src/frontend/plumos_mali_renderer.h`, `src/frontend/plumos_controller_ui.c`, `src/probe/plumos_fb_restore.c` | `frontend`, `scripts/capture-a30-framebuffer.sh` | [A30 runtime probe](../a30-runtime-probe.ja.md), [A30 stock SDL video](../a30-stock-sdl-video.ja.md) |
| boot splash | `/mnt/SDCARD/plumos/config/frontend/boot-splash.png` | `src/frontend/plumos_boot_splash.c`, `package/frontend/plumos/config/frontend/boot-splash.png` | `frontend` | [MainUI bootstrap](../mainui-bootstrap.ja.md) |
| theme layout と画像 asset | `/mnt/SDCARD/plumos/themes/` | `package/frontend/plumos/themes/*/theme.json`, `package/frontend/plumos/themes/default/images/` | `frontend`, release package rebuild | [frontend theme model](../frontend-theme-model.ja.md), [A30 UI design](../a30-ui-design.ja.md) |
| 多言語化と UI ラベル | `/mnt/SDCARD/plumos/share/frontend/lang/*.lang` | `package/frontend/plumos/share/frontend/lang/*.lang`, `src/frontend/` 内の UI label key | `frontend` | [A30 settings policy](../a30-settings-policy.ja.md) |

## System、ROM List、Launch Profile

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| system 一覧、ROM directory、拡張子、default core | `/mnt/SDCARD/plumos/config/frontend/systems.json` | `package/frontend/plumos/config/frontend/systems.json`, `scripts/generate-systems-json-from-libretro-map.py` | `frontend`, `scripts/audit-launch-profiles.py` | [対応システム](../user/emulators.ja.md), [FE 実行可能ターゲット](../emulator-fe-targets.ja.md) |
| Apps menu entry | `/mnt/SDCARD/plumos/config/frontend/apps.json` | `package/frontend/plumos/config/frontend/apps.json` | `frontend` | [frontend data model](../frontend-data-model.ja.md) |
| menu と Settings 階層 | `/mnt/SDCARD/plumos/config/frontend/menus.json` | `package/frontend/plumos/config/frontend/menus.json`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 settings policy](../a30-settings-policy.ja.md) |
| core select、system/ROM 別 override | `/mnt/SDCARD/plumos/state/frontend/core-overrides.json` | `src/frontend/plumos_controller_ui.c`, `/mnt/SDCARD/plumos/bin/` の launcher | `frontend`, emulator runtime package | [emulator runtime manifest](../emulator-runtime-manifest.ja.md), [emulator issue triage](../emulator-issue-triage.ja.md) |
| favorites、recent、cursor 復帰 | `/mnt/SDCARD/plumos/state/frontend/` | `src/frontend/plumos_library_scan.c`, `src/frontend/plumos_controller_ui.c` | `frontend` | [frontend data model](../frontend-data-model.ja.md) |
| directory-as-game と subdirectory 移動 | `/mnt/SDCARD/Roms/` 配下 | `src/frontend/plumos_library_scan.c` | `frontend` | [frontend prototype](../frontend-prototype.ja.md) |

## Artwork Scraping

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| thumbnail scraping command | `/mnt/SDCARD/plumos/bin/plumos-thumbnail-scraper` | `package/frontend/plumos/bin/plumos-thumbnail-scraper` | `frontend`, `scripts/benchmark-a30-thumbnail-scraper.sh` | [ユーザー向け scraping guide](../user/scraping.ja.md), [artwork scraper prototype](../artwork-scraper-prototype.ja.md), [thumbnail scraping results](../thumbnail-scraping-results.ja.md) |
| scraper source DB と system enable | `/mnt/SDCARD/plumos/config/frontend/scraper-*.tsv`, `systems.json` | `package/frontend/plumos/config/frontend/scraper-sources.tsv`, `scraper-rescue-seeds.tsv`, `systems.json` | `scripts/prefetch-thumbnail-scraper-cache.sh`, `scripts/build-thumbnail-rescue-overlays.py` | [third-party data](../third-party-data.ja.md) |
| Images 出力 directory 方針 | `/mnt/SDCARD/Images/<rom_dir>/` | `plumos-thumbnail-scraper`, `systems.json` の ROM scan config | `frontend` | [ユーザー向け storage](../user/storage.ja.md), [thumbnail scraping results](../thumbnail-scraping-results.ja.md) |

## Emulator と Runtime

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| RetroArch runtime と launcher | `/mnt/SDCARD/plumos/retroarch/`, `/mnt/SDCARD/plumos/bin/plumos-retroarch-launch` | `docker/plumos-toolchain/scripts/build-retroarch-practical.sh`, `package/frontend/plumos/factory-defaults/ra/` | `./scripts/docker-build.sh retroarch-practical` | [RetroArch build options](../retroarch-build-options.ja.md), [RetroArch baseline](../retroarch-known-working-baseline.ja.md) |
| libretro core recipe と source lock | `/mnt/SDCARD/plumos/retroarch/cores/` | `docker/plumos-toolchain/libretro-core-recipes.tsv`, `docs/onion-libretro-source-lock.tsv` | `./scripts/docker-build.sh libretro-cores`, `scripts/update-libretro-built-core-inventory.py` | [libretro core version inventory](../libretro-core-version-inventory.ja.md), [emulator runtime manifest](../emulator-runtime-manifest.ja.md) |
| PicoArch runtime と A30 presenter/input patch | `/mnt/SDCARD/plumos/emulators/picoarch/`, `/mnt/SDCARD/plumos/bin/plumos-picoarch-launch` | `docker/plumos-toolchain/scripts/build-picoarch.sh`, `docker/plumos-toolchain/patches/picoarch-*.patch` | `./scripts/docker-build.sh picoarch` | [emulator issue triage](../emulator-issue-triage.ja.md), [A30 input policy](../a30-input-policy.ja.md) |
| standalone emulator launcher | `/mnt/SDCARD/plumos/bin/plumos-standalone-launch` | `docker/plumos-toolchain/scripts/build-standalone-emulators.sh`, `package/frontend/plumos/config/frontend/systems.json` | `./scripts/docker-build.sh standalone-emulators` | [emulator runtime manifest](../emulator-runtime-manifest.ja.md), known-good baseline docs |
| PPSSPP defaults と復旧基準 | `/mnt/SDCARD/plumos/emulators/ppsspp/`, `/mnt/SDCARD/plumos/state/standalone/ppsspp/` | `docker/plumos-toolchain/patches/ppsspp-*.patch`, `package/frontend/plumos/factory-defaults/sa/` | `standalone-emulators`, `plumos-factory-reset` | [PPSSPP baseline](../ppsspp-known-good-baseline.ja.md) |
| ScummVM standalone | `/mnt/SDCARD/plumos/emulators/scummvm/`, `/mnt/SDCARD/plumos/state/standalone/scummvm/` | `docker/plumos-toolchain/patches/scummvm-*.patch`, `build-standalone-emulators.sh` | `standalone-emulators` | [ScummVM baseline](../scummvm-known-working-baseline.ja.md) |
| Pyxel runtime | `/mnt/SDCARD/plumos/python/`, `/mnt/SDCARD/plumos/bin/plumos-pyxel-a30-launch` | `docker/plumos-toolchain/scripts/build-python-runtime.sh`, `build-pyxel-a30.sh`, `pyxel-a30-shim/`, SDL3 `a30mali` patch | `./scripts/docker-build.sh python-runtime`, `pyxel-a30` | [Pyxel A30 output](../pyxel-a30-output.ja.md) |
| OpenBOR、EasyRPG、PCSX-ReARMed | `/mnt/SDCARD/plumos/emulators/<name>/` | `build-standalone-emulators.sh`, `docker/plumos-toolchain/patches/` の対応 patch | `standalone-emulators` | [emulator runtime manifest](../emulator-runtime-manifest.ja.md) |

## 入力、Hotkey、Power、Device State

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| A30 物理入力と virtual gamepad | `/mnt/SDCARD/plumos/bin/plumos-joystickd` | `src/input/plumos_joystickd.c`, `src/input/plumos_joystick_reader.c` | `./scripts/docker-build.sh joystickd`, `scripts/probe-a30-joystickd-*.sh` | [A30 joystickd](../a30-joystickd.ja.md), [A30 input policy](../a30-input-policy.ja.md) |
| safe hotkey、emulator power menu、volume key | `/mnt/SDCARD/plumos/bin/plumos-safe-hotkeyd`, `plumos-power-menu-overlay` | `src/input/plumos_safe_hotkeyd.c`, `package/frontend/plumos/bin/plumos-power-menu-overlay` | `joystickd`, `frontend` | [A30 input policy](../a30-input-policy.ja.md), [A30 system notes](../a30-system-notes.ja.md) |
| sleep、shutdown、poweroff flow | `/mnt/SDCARD/plumos/bin/plumos-safe-shutdown` | `package/frontend/plumos/bin/plumos-safe-shutdown`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 system notes](../a30-system-notes.ja.md) |
| volume と brightness runtime control | `/mnt/SDCARD/plumos/bin/plumos-volume-control`, system settings | `package/frontend/plumos/bin/plumos-volume-control`, `package/frontend/plumos/config/system/settings.json` | `frontend` | [A30 settings policy](../a30-settings-policy.ja.md), [config layout policy](../plumos-config-layout.ja.md) |
| CPU 周波数/core policy | `/sys/devices/system/cpu/`, FE launch environment | `/mnt/SDCARD/plumos/bin/` の launcher, `src/frontend/plumos_controller_ui.c` | `frontend`, emulator runtime build | [A30 system notes](../a30-system-notes.ja.md), [A30 settings policy](../a30-settings-policy.ja.md) |

## Network と外部アクセス

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| SSH/SFTP service | `/mnt/SDCARD/plumos/ssh/`, `/mnt/SDCARD/plumos/ssh/start-ssh.sh` | `package/ssh-kit/`, `scripts/build-ssh-kit.sh` | `./scripts/build-ssh-kit.sh`, runtime package rebuild | [SSH bring-up](../ssh-bringup.ja.md), [network services](../network-services.ja.md) |
| FTP/SFTP/Samba toggle | `/mnt/SDCARD/plumos/bin/plumos-network-services` | `package/network-services/`, `package/frontend/plumos/bin/plumos-network-control` | `./scripts/docker-build.sh network-services`, `frontend` | [network services](../network-services.ja.md) |
| Wi-Fi status/recovery | `/mnt/SDCARD/plumos/bin/plumos-network-rescue` | `package/frontend/plumos/bin/plumos-network-rescue`, `src/frontend/plumos_controller_ui.c` | `frontend` | [A30 system notes](../a30-system-notes.ja.md), [network services](../network-services.ja.md) |
| USB Disk Mode | `/mnt/SDCARD/plumos/bin/plumos-usb-disk-mode` | `package/network-services/`, `package/frontend/plumos/bin/plumos-sdcard-cleanup` | `network-services`, `frontend` | [USB Disk Mode](../usb-disk-mode.ja.md) |

## Apps

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| file manager | `/mnt/SDCARD/plumos/apps/nextcommander/` | `docker/plumos-toolchain/scripts/build-nextcommander.sh`, `nextcommander-a30.patch`, `apps.json` | `./scripts/docker-build.sh nextcommander`, `frontend` | [開発者向けガイド](README.ja.md) |
| music player | `/mnt/SDCARD/plumos/apps/music-player/` | `src/apps/plumos_music_player.c`, `docker/plumos-toolchain/scripts/build-music-player.sh`, `apps.json` | `./scripts/docker-build.sh music-player` | [plumOS music player](../plumos-music-player.ja.md) |
| RetroArch / PPSSPP standalone app entry | Apps menu | `package/frontend/plumos/config/frontend/apps.json`, `/mnt/SDCARD/plumos/bin/` の launch script | `frontend`, runtime package rebuild | [基本操作](../user/operation.ja.md) |

## Boot、Packaging、Release

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| MainUI wrapper と StockOS fallback | `/mnt/SDCARD/miyoo/app/MainUI`, `MainUI.stock` | `package/bootstrap/plumos/bootstrap/MainUI.wrapper`, `scripts/build-bootstrap-package.sh`, `scripts/install-mainui-wrapper-a30.sh` | `./scripts/build-bootstrap-package.sh`, `scripts/a30-fe-control.sh` | [MainUI bootstrap](../mainui-bootstrap.ja.md) |
| runtime package | `/mnt/SDCARD/plumos/` | `scripts/build-runtime-package.py` と各 `dist/plumos-*` input | `./scripts/build-runtime-package.py` | [runtime package](../runtime-package.ja.md) |
| SD-root release package | SD card root | `scripts/build-sdroot-package.py`, `scripts/audit-release-sdroot.py` | `./scripts/build-sdroot-package.py`, `.7z` archive step | [SD root package](../sdroot-package.ja.md), [release artifacts](../release-artifacts.ja.md) |
| developer package | release asset | `scripts/build-dev-package.py` | `./scripts/build-dev-package.py` | [developer package](../developer-package.ja.md) |
| release bundle | `dist/plumos-release-<version>/` | `scripts/build-release-bundle.py` | `./scripts/build-release-bundle.py --version <version>` | [release artifacts](../release-artifacts.ja.md) |
| factory reset | `/mnt/SDCARD/plumos/bin/plumos-factory-reset`, `plumos/factory-defaults/` | `package/frontend/plumos/bin/plumos-factory-reset`, `package/frontend/plumos/factory-defaults/` | `frontend`, runtime package rebuild | [config layout policy](../plumos-config-layout.ja.md), baseline docs |

## 診断と実機検証

| 機能 / 作業 | Runtime path | Source / config | Build / deploy | Reference docs |
| --- | --- | --- | --- | --- |
| A30 で command 実行 | SSH target | `scripts/run-a30.sh` | host helper | [開発者向けガイド](README.ja.md) |
| A30 へ deploy | `/mnt/SDCARD` | `scripts/deploy-a30.sh` | host helper | [Docker build guide](build.ja.md) |
| FE の安全な起動/停止/再起動 | `/dev/fb0` owner | `scripts/a30-fe-control.sh` | host helper | [A30 runtime probe](../a30-runtime-probe.ja.md) |
| stale emulator/display owner 停止 | `/dev/fb0` owner | `scripts/stop-a30-display-processes.sh` | host helper | [A30 runtime probe](../a30-runtime-probe.ja.md) |
| 画面 capture | `/dev/fb0` | `scripts/capture-a30-framebuffer.sh` | host helper | [A30 runtime probe](../a30-runtime-probe.ja.md) |
| log と状態回収 | `/mnt/SDCARD/plumos/logs/` | `scripts/collect-a30-logs.sh`, `scripts/collect-a30-info.sh` | host helper | [A30 system notes](../a30-system-notes.ja.md) |
| display/input/runtime path probe | probe binary と script | `src/probe/`, `scripts/probe-a30-*.sh` | 対応する `docker-build.sh` target | [A30 runtime probe](../a30-runtime-probe.ja.md), [A30 stock SDL video](../a30-stock-sdl-video.ja.md) |

## Quick Build Target Map

| 変更したもの | 通常 rebuild するもの |
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
