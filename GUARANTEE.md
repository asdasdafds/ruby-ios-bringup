# Reproducibility guarantee

**English** · [中文](GUARANTEE.zh.md)

**Status 2026-10-21 — documentation-level (theoretical) guarantee.**
This is a documented expectation for the success path, not an independently
reproduced device guarantee. Detailed test guarantees are deferred due to time
and effort.

## Scope

Only the success path is covered:

```
stock LK -> XNU 8796 -> iOS 16.7.16 userland
  -> lock screen -> synthetic swipe -> SpringBoard home screen
```

Failed routes are historical evidence, not covered by this guarantee.

## Definition

Given all of the following:

- the same device situation (ruby / MT6877, Android 12 / MIUI 13.0.4, unlocked,
  slot `_a`, the partition layout in `docs/en/device.md`);
- the same boot image and iOS root/ICL state (hashes below);
- the boot args in `config/0084-boot.cmdline`;
- the documented procedure in `docs/en/09-reproduce.md` and
  `docs/en/05-userland.md`;

the success state is **expected** to reproduce. "Expected" is the theoretical
part: no second-unit or clean-room device validation has been performed.

If any hash differs, this guarantee does not apply; treat it as a different
configuration.

## Success bundle

| Item | Value | Note |
|---|---|---|
| Baseline | `MakrSas/ios-topaz@5095ac1` | upstream commit |
| Patchset | `de1824c2...` | 87 files; recipe output |
| 0010/0012 change | `1edb6633...` | small baseline patch |
| Reference kernel | `b38fca5f...` | the kernel that booted |
| CI rebuild | `3ceb9b34...` | pre-public run; not flashed |
| Boot image | `94c639d1...` | external; not in the repo |
| Root image | `985c30fc...` | external; recorded 2026-10-21; verify before use |
| ICL original block | `ae317804...` | restore path |
| ICL patched block | `4a489542...` | bypass |
| Boot args | `config/0084-boot.cmdline` | exact boot23 cmdline |
| Success criterion | first boot ~60 s (58 s measured); warm reboot ~22 s; swipe to home; animations | author's boot22/boot23 |

Full hashes:

```text
patchset   de1824c28b5e56f7c60988c33501cd5bbfd24a4159d9358223fd2af11392d360
0010/0012  1edb66332bb4daa8b1abcd6c4de77a0e88ed9c7e9bb4a2a7045a2696e53ab338
kernel     b38fca5f38cfb721e21228eac4dd4b355000d1b850bdf204afba9ff8b8d1afcd
ci kernel  3ceb9b3490f8c8ecaa74348c3c5cb02d7302d0ec7f87006fe8354e9d2011a831
boot       94c639d1f45197718c62bd09c32c72b39710ba23de60f4a04bba96014bce1cb6
root       985c30fcca28cb79e6ad6d30a1ff4b4b6b7306b69d6e480cd7cccf4fb5ca11e3
icl orig   ae3178046fe7cb9e08291c39dc4773429be26c35472c6fbd9135d5f2a225fd73
icl pat    4a4895422dcec766b90b65c067ecb06ef8fd3e346c03ee7e2464a85f62a73b74
```

The root and boot images are **not** included: they contain Apple/Android
material that cannot be redistributed. The hashes are the contract.

Device-baseline hashes (GPT/boot/vbmeta) are intentionally **not** published;
they can fingerprint the unit. The device situation is described in
`docs/en/device.md`; only the artifact hashes above are the contract.

## Guaranteed

- The patchset and recipe: the workflow refuses to build unless the recipe
  reproduces `de1824c2...` (87 files); CI is green.
- The offline tools: host checks, synthetic and real ICL block round-trips.
- The documented success-path procedure and its scope.
- Historical evidence for failed routes, clearly marked as not supported.

## Rebuild

`scripts/build-payload.sh` + `tools/image/pack_boot.py` with the inputs above
and seed `0x84` produce `a0ce0fe1...`. The rebuild is layout-equivalent to the
original image, **not** byte-identical: the original used `os.urandom` filler.
Use `tools/image/boot_layout.py` to compare a rebuild; the success image
remains `94c639d1...`.

## Not guaranteed

- An independent device boot on another unit.
- Exact UI pixels, timing or app state.
- Boot of the CI-rebuilt kernel (it has not been flashed).
- Regression of the original (unpatched) ICL block.
- The root/boot artifacts themselves (not included).
- A byte-identical rebuild of the original boot image (its filler was random).
- Any failed route's device behavior.

## Deferred validation

Deferred due to time and effort, not because they are known to fail:

- second-unit boot test;
- Linux clean-room host CI;
- flashing and booting the CI-rebuilt kernel;
- original ICL block regression;
- real touch, keys and USB;
- long-run stability;
- device regression tests: none were run for this packaging (author decision,
  2026-10-21).

Device-baseline hashes (GPT, boot_a/b, vbmeta) are intentionally not
published; the device is described in `docs/en/device.md` instead.

These are explicit open items. Until they are done, this document remains a
**theoretical/documentation-level guarantee**.

## How to use

1. Compare your device situation and artifact hashes with the table above.
2. Follow `docs/en/09-reproduce.md` and `docs/en/05-userland.md`.
3. If a hash differs, stop: it is a different configuration, not a covered
   reproduction.
4. If you perform an independent run, record the hashes and result (sanitized)
   and update this document.

## Failed routes

`patches/historical/0081-0083` and the failure catalogue in
`docs/en/07-experiments.md` are kept as evidence. They are **not** part of the
success build, are not re-tested on a device, and are not guaranteed.
