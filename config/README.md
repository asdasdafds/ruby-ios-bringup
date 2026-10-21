# Config

`0084.env` and `0084-boot.cmdline` record the exact state that reached the
SpringBoard home screen on 2026-10-21. They are provenance, not universal
defaults:

- `cpus=6` selects the six Cortex-A55 cores on this `ruby` unit.
- `ufsroot=iosroot`, `ufslun=2` and `sweethw=0` describe the author's
  partition layout.
- `sweetautoswipe=90000` arms the synthetic home swipe 90 s after boot.
- `sweetfb_hz=30` and `sweetfb_noconsole=1` are display/debug choices.

The kernel SHA and image SHA in `0084.env` are from the original build and are
a reference, not a reproducibility gate. The boot image itself is not included.

The workflow runs `ci/prepare-baseline.sh` and gates on the canonical patchset
hash; it does not pack the boot image. See `../ci/README.md` and
`../src/bringup/`.

Device-baseline hashes (GPT/boot/vbmeta) are intentionally **not** published;
they can fingerprint the unit. The device situation is described in
`../docs/en/device.md` and `../docs/zh/device.md`. Artifact hashes stay here.

Host pipeline pins: `SHIM_SHA256`, `PAYLOAD_SHA256`, `ADT_SHA256` and
`BASE_BOOT_SHA256` are external inputs; `PAYLOAD_OFF=0x14000`;
`REBUILD_IMAGE_SHA256` is the deterministic packer output (layout-equivalent to
the booted image, not a replacement for `IMAGE_SHA256`).

The `PAGE`/`KERNEL_SIZE`/`RAW_LEN`/`SHIM_SIZE`/`SWEETXNU_*` values are the
expected layout for `boot_layout.py --config`; they remove the golden-image
dependency.
