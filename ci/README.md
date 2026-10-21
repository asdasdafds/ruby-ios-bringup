# Build provenance (0084)

`.github/workflows/xnu.yml` reproduces the kernel that booted to the SpringBoard
home screen on 2026-10-21 (commit `00d46e7`) **without vendoring the upstream
tree**.

## Recipe

1. Fetch `MakrSas/ios-topaz` at the pinned commit `5095ac1`.
2. Remove the 24 topaz-specific patches by name
   (`ci/topaz-patches-to-remove.txt`).
3. Add this project's patches: 0080 UFS, 0084 autoswipe, and historical
   0081-0083.
4. Apply `ci/baseline-changes.patch`, the small 0010/0012 change needed for
   the ruby framebuffer addresses.
5. Normalize every `.patch`/`.sh` to LF.
6. Compute the canonical patchset hash over the sorted per-file SHA256 values
   and refuse to build unless it equals
   `de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360`
   (87 files).
7. Fetch `blacktop/darwin-xnu-build` at `6639065`, copy the patchset, and
   build on `macos-14` with Xcode 15.0.1.

The full upstream tree and `ci/ruby-baseline.diff` are **not** in the repo.
The patchset hash is the build gate; the recipe fails closed on any drift.

## Pins

| Item | Value |
|---|---|
| Baseline | `MakrSas/ios-topaz` @ `5095ac19dbdcf37b64b81fe443f4027ad51b84bf` |
| Patchset | `de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360` (87 files) |
| 0010/0012 change | `ci/baseline-changes.patch`, SHA256 `1edb6633...` |
| XNU harness | `blacktop/darwin-xnu-build` @ `663906528d1cd5ea8fde7de6f16de73db9511549` |
| Runner | `macos-14`, Xcode 15.0.1, macOS 13.5 xnu |
| Config | `DEVELOPMENT`, `MACHINE_CONFIG=SWEET`, `-mcpu=cortex-a55` |
| Reference kernel | `b38fca5f...` |
| Reference boot image | `94c639d1...` |

The reference SHAs are from the original build and are not a byte-for-byte
reproducibility gate. The patchset hash is the gate.

CI status: the kernel build was green before the final docs/template-only
amendment; see the Actions tab. The rebuilt kernel SHA256 was `3ceb9b34...`
(not flashed).

## Local check

```sh
sh ci/prepare-baseline.sh /tmp/base
python3 ci/patchset_hash.py /tmp/base/xnu/patches
```

On Windows, set `PYTHON=python`. The recipe normalizes line endings, so the
hash is the same on Windows and macOS. The target directory must not exist.

## Scope

The workflow builds the **kernel** only. The shim, ADT and 128 MiB boot image
packing are separate; see `src/bringup/` and `config/0084.env`. Apple firmware,
the iOS root and device-side boot images are not included.

## History and licensing

The public history must not contain `ci/ruby-baseline.diff` or any full
upstream delta. `baseline-changes.patch` contains only this project's small
0010/0012 change; the upstream tree is fetched, not redistributed. The upstream
projects had no license file at the pinned commits; obtain permission or keep
the recipe private before public redistribution. See `LEGAL.md`.
