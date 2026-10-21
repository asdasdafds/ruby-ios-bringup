# Build provenance (0085)

`.github/workflows/xnu.yml` reproduces the kernel that first provided real
Goodix touch on 2026-10-22 (harness commit `8716a06`) **without vendoring the
upstream tree**.

## Recipe

1. Fetch `MakrSas/ios-topaz` at the pinned commit `5095ac1`.
2. Remove the 24 topaz-specific patches by name
   (`ci/topaz-patches-to-remove.txt`).
3. Add this project's patches: 0080 UFS, 0084 autoswipe, 0085 Goodix SPI
   touch, and historical 0081-0083.
4. Apply `ci/baseline-changes.patch`, the small 0010/0012 change needed for
   the ruby framebuffer addresses.
5. Normalize every `.patch`/`.sh` to LF.
6. Compute the canonical patchset hash over the sorted per-file SHA256 values
   and refuse to build unless it equals
   `cf4008e309cfa566e3df968592a2d3f3224a9b3d0813d6054326115fecc0a03f`
   (88 files). This is the same 88-file patchset that built the booted
   0085 kernel in the harness.
7. Fetch `blacktop/darwin-xnu-build` at `6639065`, copy the patchset, and
   build on `macos-14` with Xcode 15.0.1.
8. Hash the built Mach-O with `ci/kernel_hash.py`. The gate compares the
   **exec-data** hash (`__TEXT_EXEC` plus the data segments, excluding
   `__TEXT` cstrings and `__LINKEDIT`) with the booted artifact's
   `e4823afb...`. Full-file hashes may differ between checkouts because of
   DWARF/path metadata; the patchset hash and the exec-data hash are the
   reproducibility gates.

The full upstream tree and `ci/ruby-baseline.diff` are **not** in the repo.

## Pins

| Item | Value |
|---|---|
| Baseline | `MakrSas/ios-topaz` @ `5095ac19dbdcf37b64b81fe443f4027ad51b84bf` |
| Patchset | `cf4008e309cfa566e3df968592a2d3f3224a9b3d0813d6054326115fecc0a03f` (88 files) |
| 0010/0012 change | `ci/baseline-changes.patch`, SHA256 `1edb6633...` |
| XNU harness | `blacktop/darwin-xnu-build` @ `663906528d1cd5ea8fde7de6f16de73db9511549` |
| Runner | `macos-14`, Xcode 15.0.1, macOS 13.5 xnu |
| Config | `DEVELOPMENT`, `MACHINE_CONFIG=SWEET`, `-mcpu=cortex-a55` |
| Booted kernel | `9eb9acac...` (harness, same patchset) |
| Booted kernel exec-data | `e4823afb...` (gate) |
| Booted boot image | `ace60d16...` |
| CI run (2026-10-22) | `39694456200`; kernel full `904e10df...`, exec-data `e4823afb...` (matches), image `54d1b306...` (packed, not flashed) |

The reference SHAs are provenance, not a byte-for-byte guarantee for a future
run. The patchset hash and the exec-data hash are the gates; record the actual
full artifact hash for each run in `config/0085.env`.

## Local check

```sh
sh ci/prepare-baseline.sh /tmp/base
python3 ci/patchset_hash.py /tmp/base/xnu/patches
python3 ci/kernel_hash.py /path/to/kernel.development.sweet
```

On Windows, set `PYTHON=python`. The recipe normalizes line endings, so the
hash is the same on Windows and macOS. The target directory must not exist.

## Scope

The workflow builds the **kernel** only. The shim, ADT and 128 MiB boot image
packing are separate; see `src/bringup/` and `config/0085.env`. Apple firmware,
the iOS root and device-side boot images are not included.

## History and licensing

The public history must not contain `ci/ruby-baseline.diff` or any full
upstream delta. `baseline-changes.patch` contains only this project's small
0010/0012 change; the upstream tree is fetched, not redistributed. The upstream
projects had no license file at the pinned commits; obtain permission or keep
the recipe private before public redistribution. See `LEGAL.md`.
