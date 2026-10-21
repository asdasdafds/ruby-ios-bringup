# 08 — Recovery

**English** · [中文](../zh/08-recovery.md)

Recovery is an engineering constraint, not a command list. On `ruby` there is
no recovery partition, the LK has no `boot:` command, and slot b is broken;
`boot_a` is the only handle. Keep a known-good full boot image before every
experiment.

## Preconditions

- Unlocked bootloader, slot `_a`.
- A known-good full 128 MiB Magisk/stock image on the host.
- A12 TWRP for `ruby` (not A14; not another device).
- Do **not** flash `preloader`, `lk`, `persist`, `nvram`, `nvdata`.
- Assume `userdata` can be lost; back up what you cannot recreate.

## Why the recovery handle is `boot_a`

LK reads `boot_a/b`, decompresses the kernel, loads the ramdisk and DTB, and
jumps. There is no recovery partition to boot separately, and the LK command
table has no `boot:` for a RAM-loaded recovery. Writing TWRP to `boot_a` is the
only way to get a recovery environment; writing a known-good full image back is
the only way out.

## Android restore: what the write does

The full image contains the stock/Magisk kernel, ramdisk, DTB and AVB footer.
Flashing it to `boot_a` and setting the active slot restores the original boot
path; it does not touch `userdata`.

```sh
adb reboot bootloader
fastboot devices
fastboot flash boot_a <magisk-full.img>
fastboot --set-active=a
fastboot reboot
```

Verify the result, not just the flash command:

```sh
adb devices
adb shell getprop sys.boot_completed   # 1
adb shell getprop ro.boot.slot_suffix  # _a
adb shell su -c id                     # uid=0(root)
```

If fastboot says `no link`, the LK session is gone. Power-cycle (10–15 s),
enter fastboot with volume-down + power, and flash immediately. Use a rear USB
port, no hub.

## TWRP for offline I/O

- No recovery partition and no `fastboot boot`; flash TWRP to `boot_a`.
- ADB usually comes up 20–25 s after TWRP starts.
- Raw `dd` is the reliable path for 16 GiB images; `fastboot flash` sparse
  writes can fail on `iosroot`.
- Unmount both `/data` and `/sdcard` before resizing F2FS; clear the F2FS
  checkpoint with a read-write mount and a clean unmount.
- `sync` after a raw write; verify the SHA.

```sh
# TWRP example: write a root image to iosroot
dd if=<root.img> of=/dev/block/by-name/iosroot bs=4M
sync
```

## Restore the ICL block

The NOP is reversible. If it causes problems, write the original 4K block back
and verify the block SHA before and after.

```sh
dd if=<icl_block_orig.bin> of=/dev/block/by-name/iosroot \
  bs=4096 seek=2438140 count=1 conv=notrunc
sync
```

The original/patched hashes are in `05-userland.md`.

## LK logs

Dump `expdb` and read the signatures, not the whole log:

| Signature | Meaning |
|---|---|
| `jump to K64` | the image was really entered |
| `hdr_loaded` / `img_loaded` / `can't find v2 dtb` | LK rejected the image |
| `decompress kernel` | gzip stream problem |
| `panic` | specific assert |
| `mt_disp_update:` / `OVL` | actual scan buffer |

## Hard rules

- Never flash an experiment without a known-good restore image.
- Never write to a mounted filesystem with `blkpatch`.
- Never use `userdata` as a backup area for the iOS root.
- Never publish device dumps, serials, IMEI/MAC or Apple firmware.
