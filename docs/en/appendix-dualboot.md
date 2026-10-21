# Appendix — Dual-boot layout (method note)

**English** · [中文](../zh/appendix-dualboot.md)

This is **not** an installer and **not** the default path. It is a method note
for keeping Android data while adding a separate iOS root. No automation is
provided.

## Default path

Write the iOS root into `userdata` (or a dedicated partition) with the upstream
tooling. It is simpler, but it can erase Android data. Assume data loss.

## Dual-boot example

On the author's device: `userdata` 208.4 GiB + `iosroot` 17 GiB; the HFS+ root
lives in `iosroot`, and `boot_a` switches between Android and XNU. These sizes
are an example, not a recipe for another firmware.

## Why it is not automated

- GPT edits and raw `dd` are device-specific and irreversible without a backup.
- `ruby` has no recovery partition and LK has no `boot:`; TWRP must be flashed
  to `boot_a` for offline work.
- A wrong size or block count can destroy `userdata` or brick the device.
- The method does not make the data "safe"; it only avoids the initial erase.

## Outline

1. Back up GPT, `boot_a/b`, `vbmeta`, `persist`, `modemst1/2`, `fsg`, `fsc`,
   and data you cannot recreate.
2. Choose the new `userdata` and `iosroot` sizes; keep alignment and
   `flashinfo` intact.
3. Edit primary and backup GPT; verify with a read-only dump.
4. Resize the F2FS `userdata`: clear the checkpoint (rw mount + clean unmount)
   before `resize.f2fs`.
5. Write the HFS+ root to `iosroot` with TWRP raw `dd`; verify SHA.
6. Boot XNU with `rd=ufsroot0 ufsroot=iosroot ufsrw=1 rootrw=1`; keep a
   known-good `boot_a` image to switch back.
7. Keep the original GPT and the first blocks of every changed partition until
   the layout is proven.

## If you only need iOS

Use the default path and accept the data loss. If you need Android data, the
dual-boot method is possible, but you own the risk.
