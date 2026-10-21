# TWRP-only block tools

`blkdiff` and `blkpatch` are freestanding static AArch64 binaries for TWRP.
They read and write raw block devices; they are not host tools and are not
safe to run on a mounted filesystem.

They were used across multiple boot/diff cycles during the bring-up. The diff
data itself is not included.

## Guard

Always run them through the guard, which refuses to start outside TWRP:

```sh
sh twrp-guard.sh ./blkdiff /dev/block/by-name/iosroot crc.bin diff.bin diff.idx 16384
```

`twrp-guard.sh` checks `ro.twrp.version` and then execs the command. It does not
validate the target device or partition.

## Usage

```sh
# compare a 16 GiB image with a 16 MiB CRC32 map (256 blocks per MiB)
./blkdiff IMAGE CRC DIFF IDX [TOTAL_MB]

# write diff blocks at their recorded indices
./blkpatch DEVICE DIFF IDX COUNT
```

`IDX` is a little-endian u32 array of 4K block indices; `DIFF` is the
corresponding 4K blocks in the same order. `blkpatch` seeks to each index and
writes one block.

## Build

```sh
CC=/path/to/clang ./build.sh
```

The expected target is `aarch64-linux-gnu`, `-nostdlib -static -no-pie`,
entry `_start`, lld as the linker. The resulting binaries are small and have no
libc dependency.

## Safety

- Back up the target blocks before writing; `blkpatch` does not create a backup.
- Never point `blkpatch` at a mounted filesystem or at a partition you have not
  identified by name and size.
- Run `sync` after a write.
- Verify with a second `blkdiff` against the expected CRC map.

## Host helpers

The C tools above run in TWRP. The two Python helpers prepare their inputs on
the host and never write the source image:

- `make_crc_map.py IMAGE MAP [--replace-idx IDX --replace-data DATA] [--override-block N --override-data BLOCK]`
- `make_target_patch.py IMAGE IDX OUT --crc-map MAP [replacement options]`

`MAP` is 4 bytes little-endian per 4 KiB block (256 blocks/MiB). The
replacement layer builds a variant target without a full 16 GiB copy (for
example, a pre-boot root). `make_target_patch.py` verifies every block against
the map before writing.
