# plumOS A30

Development workspace for `plumOS`, a custom firmware project for the Miyoo A30.

The project goal is to avoid depending on the stock A30 userland as much as
possible. Frontend code, libraries, RetroArch, libretro cores, configuration,
and logs should live under `/mnt/SDCARD/plumos` on the SD card.

The intended development loop is to build A30-targeted files inside Docker,
transfer them to the device over SSH, validate on real hardware, collect logs,
and iterate. Final deliverables should include both the SD-card runtime package
and developer-facing Docker/toolchain files.

## Current Status

The first milestone, reliable SSH access, is working through a small
Dropbear-based development kit. With SSH access available, we collected the A30
boot flow, mount layout, Wi-Fi behavior, stock frontend behavior, and RetroArch
layout.

Project notes:

- [A30 system notes](docs/a30-system-notes.en.md)
- [plumOS design policy](docs/plumos-design-policy.en.md)
- [Docker toolchain](docs/docker-toolchain.en.md)
- [SSH bring-up](docs/ssh-bringup.en.md)
- [TODO](TODO.en.md)

Japanese documents are the primary working documents and use the same names
without `.en.md`.

## Build SSH Kit

```sh
./scripts/build-ssh-kit.sh
```

The script downloads Dropbear, verifies the pinned SHA-256, cross-compiles it
with Zig for `arm-linux-musleabihf`, then assembles:

```text
dist/plumos-a30-ssh-kit/
dist/plumos-a30-ssh-kit.tar.gz
```

Before copying to the SD card, place your workstation public key in:

```text
dist/plumos-a30-ssh-kit/plumos/ssh/etc/authorized_keys
```

Then copy the contents of `dist/plumos-a30-ssh-kit/` to the root of the A30 SD
card. On the device, launch `Start SSH` from Ports.

Default connection:

```sh
ssh -p 2222 root@A30_IP_ADDRESS
```

## References

- Miyoo A30 toolchain reference: https://codeberg.org/hydrogen18/miyooa30
- Dropbear SSH: https://matt.ucc.asn.au/dropbear/dropbear.html
