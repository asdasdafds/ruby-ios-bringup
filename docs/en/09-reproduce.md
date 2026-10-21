# 09 — Reproduce

**English** · [中文](../zh/09-reproduce.md)

> The success-path scope, bundle hashes and non-guarantees are in
> [`../../GUARANTEE.md`](../../GUARANTEE.md).

The kernel build is reproducible. The boot image and the iOS root are not fully
self-contained here: Apple's userland is not distributable, and the 0084 image
used the upstream shim/loader/ADT builder, which is not vendored. This chapter
states what you can reproduce from this repo and what you must supply.

## What you must supply

- An unlocked `ruby` (Redmi Note 12 Pro 5G / MT6877) on Android 12.
- An iPhone X iOS 16.7.16 (20H392) IPSW and an iOS root image.
- A known-good full 128 MiB Magisk/stock boot image for recovery.
- A12 TWRP for `ruby` for offline I/O.
- A GitHub account for the macOS kernel workflow.

## Build the kernel

`.github/workflows/xnu.yml` runs `ci/prepare-baseline.sh`. The recipe fetches
the pinned baseline, removes the topaz patches, adds this project's patches,
applies the small 0010/0012 change, normalizes line endings, and refuses to
build unless the canonical patchset hash matches.

```text
base:     MakrSas/ios-topaz @ 5095ac19dbdcf37b64b81fe443f4027ad51b84bf
recipe:   ci/prepare-baseline.sh (no upstream tree vendored)
patchset: de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360
dxb:      blacktop/darwin-xnu-build @ 663906528d1cd5ea8fde7de6f16de73db9511549
runner:   macos-14, Xcode 15.0.1, macOS 13.5 xnu
config:   DEVELOPMENT, MACHINE_CONFIG=SWEET, -mcpu=cortex-a55
```

Run it from **Actions → xnu-0084 → Run workflow**, or push a change under
`ci/`, `patches/`, `config/` or the workflow. Artifacts: `xnu-kernel` and
`xnu-buildtree`.

## Host pipeline

The host pipeline uses <1 GiB artifacts and never touches the 16 GiB root. The
shim and ADT are external; the upstream payload tools are fetched at the pinned
baseline commit.

```sh
sh scripts/build-payload.sh --kernel <kernel> --adt <adt> \
  --cmdline-file config/0084-boot.cmdline --out <payload.bin>

python tools/image/pack_boot.py --base <magisk.img> --shim <Image> \
  --payload <payload.bin> --out <boot.img>

python tools/image/boot_layout.py --image <boot.img> \
  --config config/0084.env --payload <payload.bin> --base <magisk.img>
```

`--golden <0084.img>` is optional; the config pins are enough for an external
user without the golden image. The root is prepared separately per
[`root-recipe.md`](root-recipe.md).

Or run all three with `scripts/test-pipeline.sh`.

The 0084 inputs produce payload
`50701e819368239252527cc61e9bb67d4d7387798272d781f0a72470751de178`.
The deterministic packer (seed `0x84`) produces
`a0ce0fe131c2a9b63da340824ad5c94db327c966c55b2dd5d7f518a15e5a0132`; it is
layout-equivalent to the original 0084 image, not byte-identical. The original
image used `os.urandom` filler; use `boot_layout.py`, not the image SHA, for a
rebuild.

No device regression tests were run for this packaging; the home-screen result
is historical (boot23) and CI covers the build only.

## Pack the boot image

The 0084 image used the upstream loader/ADT builder. This repo carries the early
`src/bringup` packers and the offline verifier, but not the upstream loader
source. Use the baseline's `shim/` and `tools/` for a full rebuild, or supply
your own packer.

Rules that must hold:

- Full 128 MiB image; keep DTB, AVB0 and AVB footer at stock offsets.
- Kernel slot exactly 16,606,685 B (FEXTRA padding, no trailing zeros).
- Payload at the expected entry; keep the ARM64 Image header.
- Verify with `tools/image/boot_verify.py` before flashing.

## Flash

Flashing writes the whole 128 MiB image to `boot_a`; it does not change the
root partition. Keep the Magisk/stock image and TWRP image available. Slot b is
not usable.

```sh
adb reboot bootloader
fastboot devices
fastboot flash boot_a <boot.img>
fastboot --set-active=a
fastboot reboot
```

## Boot args

`config/0084-boot.cmdline` is the exact boot23 cmdline. Key arguments:

| Argument | Effect |
|---|---|
| `cpus=6` | six A55 cores |
| `ufsroot=iosroot`, `ufslun=2`, `ufsrw=1`, `rootrw=1` | root device |
| `sweetfb_noconsole=1`, `sweetfb_hz=30` | display/debug |
| `sweetautoswipe=90000`, `sweetautoswiperetry=15000`, `sweetautoswipeattempts=3` | autoswipe |

## Expected behavior

- First boot after the patch: ~58 s; warm reboot: ~22 s (boot23). Upstream's
  first boot is 4–5 min.
- Lock screen: date starts at `Wednesday, December 31` (no RTC/NTP); clock
  advances; camera icon and home bar.
- Synthetic swipe unlocks to the home screen; animations run.
- Black wallpaper, `Waiting...` widgets and a Messages badge may remain.
- Touch, keys, USB, Wi-Fi, GPU and RTC are not implemented.

## Verify

```sh
python scripts/verify-local.py
python tools/image/boot_verify.py <boot.img> <base.img>
```

`boot_verify.py` prints the image SHA, page/kernel size, SWEETXNU fields, ADT
SHA and cmdline; the optional base comparison checks the tail outside the
kernel slot.

## Recovery

See [`08-recovery.md`](08-recovery.md). Keep a full restore image before every
flash; there is no recovery partition and no `fastboot boot`.
