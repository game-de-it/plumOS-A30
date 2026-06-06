# plumOS A30

Custom firmware bring-up workspace for the Miyoo A30.

The first milestone is reliable remote access. This repository currently builds
a small Dropbear-based SSH kit that can be copied to the SD card and launched
from Ports on the handheld.

## Current Focus

1. Build an armhf static Dropbear SSH server.
2. Start it from the SD card without modifying NAND/rootfs permanently.
3. Connect from the workstation and collect system information.
4. Use that information to define the next firmware TODOs.

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

Before copying to the SD card, place your public key in:

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

