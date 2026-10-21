# Verification

This file records what was actually checked, and what was not. It is updated
after each verification pass.

## Environment

- Host: Windows 10 (build 26300), Git Bash, Python 3.13.13.
- No phone attached; no flashing or device writes.
- Source artifacts live outside this repository and were read-only inputs.

## Host-verified (2026-10-21)

`python scripts/verify-local.py` with the optional external inputs:

| Check | Result |
|---|---|
| Python syntax | 14 files, pass |
| Shell syntax | 6 files, pass |
| Forbidden binary/media files | none |
| Personal paths / secrets | none |
| Synthetic ICL round-trip | pass (dry-run, apply, wrong-bytes negative case) |
| Real ICL 4K block round-trip | pass: `ae3178046fe7cb9e08291c39dc4773429be26c35472c6fbd9135d5f2a225fd73` -> `4a4895422dcec766b90b65c067ecb06ef8fd3e346c03ee7e2464a85f62a73b74` |
| Patch 0081 apply-check | pass on a local pre-0084 tree |
| Patch 0083 apply-check | pass on the same tree |
| Patch 0084 apply-check | pass on the same tree |
| Patch 0080 reverse-check | pass (already applied in that tree) |
| Patch 0082 reverse-check | pass (already applied in that tree) |
| Boot image layout dump | pass on the external 0084 image |
| Host pipeline (payload/pack/layout) | pass: payload `50701e81...`; pack seed `0x84` -> `a0ce0fe1...`; layout PASS vs 0084; only filler bytes differ |
| Strict host bundle | pass: kernel/boot/shim/payload/base/cmdline (root/ICL optional) |
| Config-mode layout | pass without golden; 11 pinned checks |
| Linux host CI | pass before the final docs/template-only amendment; see Actions |

Boot image fields reported by `tools/image/boot_verify.py`:

- image SHA256: `94c639d1f45197718c62bd09c32c72b39710ba23de60f4a04bba96014bce1cb6`
- page / kernel size: 2048 / 16,606,685
- decompressed payload: 37,449,728 bytes
- SWEETXNU: version 1, 10 segments
- ADT SHA256: `c778d4b5a559ad7c4e8f54113cc8b4e7d91d34082efc91acb02e1fcce0099720`
- cmdline includes `cpus=6`, `ufsroot=iosroot`, `ufslun=2`, `sweetfb_hz=30`,
  `sweetautoswipe=90000`, `sweetautoswipeattempts=3`

The ICL blocks, boot image and XNU tree are **not** in this repository. The
hashes above are recorded as evidence; the inputs stay outside the repository.

## Build recipe (0084)

The workflow `.github/workflows/xnu.yml` fetches `MakrSas/ios-topaz` at
`5095ac1`, runs `ci/prepare-baseline.sh`, and refuses to build unless the
canonical patchset hash is
`de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360`
(87 files). The recipe removes the topaz patches, adds this project's patches,
applies the small 0010/0012 change, and normalizes line endings.

Locally (Windows):

- the recipe was run against a fresh `topaz@5095ac1` checkout in a temp dir;
- after the recipe, `patchset_hash.py` reported `de1824c2...` with 87 files;
- `ci/baseline-changes.patch` has SHA256 `1edb6633...`;
- the workflow YAML parses;
- `config/0084.env` pins match the workflow and the patchset hash.

The full upstream tree and `ci/ruby-baseline.diff` are **not** in the clean
history. Before the final docs/template-only amendment, the kernel build and
host checks were green; see Actions. The rebuilt kernel SHA256 is
`3ceb9b3490f8c8ecaa74348c3c5cb02d7302d0ec7f87006fe8354e9d2011a831`;
the reference original is `b38fca5f...`. Builds are not bit-identical. The
rebuilt kernel was not flashed. No device regression tests were run for this
packaging.

The reference kernel/image SHAs in `config/0084.env` come from the original
build.

## Guarantee status (2026-10-21)

- **G1 — host/build:** verified. A fresh clone passes `verify-local.py`; the
  kernel workflow uses the recipe and enforces the canonical patchset hash; CI
  was green before the final docs/template-only amendment; see Actions.
- **G2 — success path:** documented, **theoretical** guarantee. The bundle is
  the patchset, boot image, root image, ICL blocks and boot args. It has not
  been independently reproduced on a second unit or in a clean-room.
- **G3 — author's device:** historical; boot23 reached the home screen with
  animations.
- **Deferred:** second-unit boot, Linux clean-room CI, flashing the rebuilt
  kernel, original ICL block regression, real touch/keys/USB, long-run
  stability.
- **No device regression tests were run for this packaging** (author decision,
  2026-10-21). Device-baseline hashes are intentionally not published.

The scope, hashes and non-guarantees are in [`GUARANTEE.md`](GUARANTEE.md).

## Device-verified (historical)

These results were observed on the author's device during the bring-up. They
are not reproduced by CI and are not a guarantee for another unit or firmware.

- boot22 (first boot after the patch): 58 s; boot23 (warm reboot): 22 s;
  synthetic autoswipe unlocked to the home screen; animations normal.
- ICL NOP: iosroot 4K block 2438140, original `ae317804...`, patched
  `4a489542...`; original block backed up.
- boot22 diff: 62,490 changed 4K blocks (244 MiB); the diff data is not in this
  repository.
- UFS root on LUN 2 and read/write HFS+ were exercised on the device.

## Not verified

- Real touch, buttons, USB, Wi-Fi, GPU and RTC.
- Regression of the original (unpatched) ICL block after the first-boot state
  was created.
- A from-scratch reproduction on a different `ruby` unit or firmware.
- A device regression test for this packaging; none was run.

## Commands

```sh
python scripts/verify-local.py
```

With external inputs (paths are yours; they are not committed):

```sh
python scripts/verify-local.py \
    --icl-orig <original-4k-block> \
    --icl-patched <patched-4k-block> \
    --xnu-tree <patched-xnu-tree> \
    --boot-image <boot.img>
```
