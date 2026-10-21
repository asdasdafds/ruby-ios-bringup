#!/usr/bin/env python3
"""Build a 4 KiB CRC32 map for tools/twrp/blkdiff.

Format: 4 bytes little-endian per 4096-byte block, 256 blocks per MiB.
The source image is read only; the map is written to OUT.

An optional replacement layer builds a variant target without a full copy:

  --replace-idx IDX --replace-data DATA
      idx is a u32 little-endian block list; data holds one 4 KiB block per idx.
  --override-block N --override-data BLOCK
      replace one block (for example the ICL block).

The tool is host-only. It does not touch a device.
"""
import argparse
import hashlib
import os
import struct
import sys
import zlib

BLK = 4096
MIB = 1 << 20


def load_replacements(idx_path, data_path):
    idx = open(idx_path, "rb").read()
    data = open(data_path, "rb").read()
    if len(idx) % 4:
        sys.exit("replacement idx size is not a multiple of 4")
    count = len(idx) // 4
    if len(data) != count * BLK:
        sys.exit("replacement data size does not match idx")
    out = {}
    for j in range(count):
        block = struct.unpack_from("<I", idx, j * 4)[0]
        out[block] = data[j * BLK:(j + 1) * BLK]
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", help="source image (read-only)")
    ap.add_argument("out", help="CRC map (4 bytes per 4 KiB block)")
    ap.add_argument("--replace-idx")
    ap.add_argument("--replace-data")
    ap.add_argument("--override-block", type=lambda v: int(v, 0))
    ap.add_argument("--override-data")
    args = ap.parse_args()

    if bool(args.replace_idx) != bool(args.replace_data):
        sys.exit("--replace-idx and --replace-data must be used together")
    if (args.override_block is None) != (args.override_data is None):
        sys.exit("--override-block and --override-data must be used together")

    repl = load_replacements(args.replace_idx, args.replace_data) if args.replace_idx else {}
    override = None
    if args.override_data is not None:
        override = open(args.override_data, "rb").read()
        if len(override) != BLK:
            sys.exit("override data must be exactly 4096 bytes")

    size = os.path.getsize(args.image)
    if size % BLK:
        sys.exit("image size is not a multiple of 4096")
    if size % MIB:
        sys.exit("image size is not a multiple of 1 MiB (blkdiff scans MiB units)")

    blocks = size // BLK
    with open(args.image, "rb") as f, open(args.out, "wb") as o:
        for b in range(blocks):
            raw = f.read(BLK)
            if len(raw) != BLK:
                sys.exit("short read at block %d" % b)
            if override is not None and b == args.override_block:
                raw = override
            elif b in repl:
                raw = repl[b]
            o.write(struct.pack("<I", zlib.crc32(raw) & 0xFFFFFFFF))
            if (b & 0x7FFFF) == 0:
                print("scanned %d/%d blocks" % (b, blocks), file=sys.stderr)

    data = open(args.out, "rb").read()
    print("map: %s" % args.out)
    print("blocks: %d" % blocks)
    print("bytes: %d" % len(data))
    print("sha256: %s" % hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    main()
