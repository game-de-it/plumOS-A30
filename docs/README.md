# plumOS Documentation Index

This `docs/` tree separates end-user documentation from developer documentation.
English `.md` files are the default GitHub-facing documents. Japanese
counterparts use `.ja.md`.

## End-User Documentation

These documents are for people installing and using plumOS on a Miyoo A30.

- [User Guide](user/README.md)
- [Installation](user/install.md)
- [Basic Operation](user/operation.md)
- [SD Card Layout](user/storage.md)
- [Save Data and States](user/save-data.md)
- [Supported Systems and Emulator Profiles](user/emulators.md)

## Developer Documentation

These documents are for people building, modifying, testing, or releasing
plumOS.

- [Developer Guide](developer/README.md)
- [Docker Build Guide](developer/build.md)
- [Feature Reverse Lookup](developer/feature-index.md)

## Detailed Design and Investigation Notes

The documents below preserve implementation detail, investigation results, and
technical decisions. User-facing documentation links to these only when the
extra detail is useful.

- [plumOS Design Policy](plumos-design-policy.md)
- [Feature Reverse Lookup](developer/feature-index.md)
- [Config Layout Policy](plumos-config-layout.md)
- [A30 UI Design](a30-ui-design.md)
- [A30 Settings Policy](a30-settings-policy.md)
- [A30 Input Policy](a30-input-policy.md)
- [A30 joystickd](a30-joystickd.md)
- [MainUI Bootstrap](mainui-bootstrap.md)
- [Frontend Data Model](frontend-data-model.md)
- [Frontend Theme Model](frontend-theme-model.md)
- [Network Services](network-services.md)
- [USB Disk Mode](usb-disk-mode.md)
- [Docker Toolchain Details](docker-toolchain.md)
- [Runtime Package](runtime-package.md)
- [SD Root Package](sdroot-package.md)
- [Release Artifacts](release-artifacts.md)
- [Emulator Runtime Manifest](emulator-runtime-manifest.md)
- [FE Executable Targets](emulator-fe-targets.md)
- [Emulator Issue Triage](emulator-issue-triage.md)
- [libretro Core Version Inventory](libretro-core-version-inventory.md)
- [Third-Party Data](third-party-data.md)
- [Repository License](../LICENSE)
- [Third-Party Notices](../THIRD_PARTY_NOTICES.md)

## Machine-Readable Tables

These files are the source data for generated or audited documentation.

- [emulator-runtime-manifest.tsv](emulator-runtime-manifest.tsv)
- [emulator-runtime-verification.tsv](emulator-runtime-verification.tsv)
- [emulator-fe-libretro-targets.tsv](emulator-fe-libretro-targets.tsv)
- [emulator-fe-standalone-targets.tsv](emulator-fe-standalone-targets.tsv)
- [libretro-built-cores.tsv](libretro-built-cores.tsv)
- [libretro-core-version-inventory.tsv](libretro-core-version-inventory.tsv)
- [onion-libretro-source-lock.tsv](onion-libretro-source-lock.tsv)

## Japanese Counterpart

- [Japanese documentation index](README.ja.md)
