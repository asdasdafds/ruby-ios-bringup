#!/usr/bin/env python3
"""Build the target block patch for tools/twrp/blkpatch.

For every block index produced by blkdiff, take the target block from IMAGE
(or from an optional replacement layer), verify its CRC32 against --crc-map,
and write the 4 KiB block to OUT. blkpatch applies OUT with the same index.

The source image is read only. The tool is host-only.
"""
import argparse
import hashlib
import struct
import sys
import zlib

BLK = 4096


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
    ap.add_argument("idx", help="u32 little-endian block index from blkdiff")
    ap.add_argument("out", help="target block patch for blkpatch")
    ap.add_argument("--crc-map", required=True, help="expected CRC map")
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

    idx = open(args.idx, "rb").read()
    if len(idx) % 4:
        sys.exit("idx size is not a multiple of 4")
    crc_map = open(args.crc_map, "rb").read()
    count = len(idx) // 4

    with open(args.image, "rb") as f, open(args.out, "wb") as o:
        for j in range(count):
            block = struct.unpack_from("<I", idx, j * 4)[0]
            if (block + 1) * 4 > len(crc_map):
                sys.exit("block %d outside the CRC map" % block)
            if override is not None and block == args.override_block:
                data = override
            elif block in repl:
                data = repl[block]
            else:
                f.seek(block * BLK)
                data = f.read(BLK)
            if len(data) != BLK:
                sys.exit("short read at block %d" % block)
            want = struct.unpack_from("<I", crc_map, block * 4)[0]
            got = zlib.crc32(data) & 0xFFFFFFFF
            if got != want:
                sys.exit("crc mismatch at block %d: %08x != %08x" % (block, got, want))
            o.write(data)
            if (j & 0x3FFF) == 0:
                print("wrote %d/%d blocks" % (j, count), file=sys.stderr)

    data = open(args.out, "rb").read()
    print("patch: %s" % args.out)
    print("blocks: %d" % count)
    print("bytes: %d" % len(data))
    print("sha256: %s" % hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    main()
