#!/usr/bin/env python3
"""Apply a 4-byte instruction patch to a raw block or a raw disk image.

The tool is generic: the block index, the in-block offset and both byte
patterns are arguments. It was written for the InstalledContentLibrary NOP
described in the project notes, but nothing here depends on that image.

Modes:
  --input-block FILE              operate on a standalone 4096-byte block
  --image FILE --block-index N    operate on block N of a raw image

The default is a dry run. --apply writes a backup first, then patches the
block, fsyncs and re-reads it for verification. A SHA mismatch aborts before
anything is written.

Examples:
  python icl_nop.py --input-block block.bin --in-block-offset 0xF0C \
      --expect-bytes 02255e95 --patch-bytes 1f2003d5

  python icl_nop.py --image root.img --block-index 2438140 \
      --in-block-offset 0xF0C --expect-block-sha <sha> \
      --expect-patched-sha <sha> --apply
"""
import argparse
import hashlib
import os
import sys

BLOCK = 4096


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def parse_hex4(value):
    raw = bytes.fromhex(value)
    if len(raw) != 4:
        raise argparse.ArgumentTypeError("expected exactly 4 bytes of hex")
    return raw


def load_block(args):
    if args.input_block:
        with open(args.input_block, "rb") as f:
            data = f.read()
        if len(data) != BLOCK:
            raise SystemExit(f"input block is {len(data)} bytes, expected {BLOCK}")
        return data
    with open(args.image, "rb") as f:
        f.seek(args.block_index * BLOCK)
        data = f.read(BLOCK)
    if len(data) != BLOCK:
        raise SystemExit(f"short read at block {args.block_index}: {len(data)} bytes")
    return data


def write_block(args, patched):
    if args.input_block:
        out = args.output or (args.input_block + ".patched")
        if os.path.exists(out) and not args.force:
            raise SystemExit(f"output exists: {out} (use --force to overwrite)")
        with open(out, "wb") as f:
            f.write(patched)
            f.flush()
            os.fsync(f.fileno())
        check = open(out, "rb").read()
        if check != patched:
            raise SystemExit(f"readback mismatch: {out}")
        print(f"wrote {out}")
        return

    backup = args.backup or f"{args.image}.block{args.block_index}.orig"
    if os.path.exists(backup) and not args.force:
        raise SystemExit(f"backup exists: {backup} (use --force to overwrite)")
    original = load_block(args)
    with open(backup, "wb") as f:
        f.write(original)
        f.flush()
        os.fsync(f.fileno())
    with open(args.image, "r+b") as f:
        f.seek(args.block_index * BLOCK)
        f.write(patched)
        f.flush()
        os.fsync(f.fileno())
    with open(args.image, "rb") as f:
        f.seek(args.block_index * BLOCK)
        check = f.read(BLOCK)
    if check != patched:
        raise SystemExit(f"readback mismatch; backup is {backup}")
    print(f"backup {backup}")
    print(f"patched {args.image} block {args.block_index}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--input-block", metavar="FILE")
    src.add_argument("--image", metavar="FILE")
    ap.add_argument("--block-index", type=int, metavar="N")
    ap.add_argument("--in-block-offset", type=lambda v: int(v, 0), default=0xF0C)
    ap.add_argument("--expect-bytes", type=parse_hex4, default=parse_hex4("02255e95"))
    ap.add_argument("--patch-bytes", type=parse_hex4, default=parse_hex4("1f2003d5"))
    ap.add_argument("--expect-block-sha", metavar="HEX")
    ap.add_argument("--expect-patched-sha", metavar="HEX")
    ap.add_argument("--backup", metavar="FILE")
    ap.add_argument("--output", metavar="FILE")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    if args.input_block and args.block_index is not None:
        ap.error("--block-index is only valid with --image")
    if args.image and args.block_index is None:
        ap.error("--image requires --block-index")
    if args.image and args.output:
        ap.error("--output is only valid with --input-block")
    if not (0 <= args.in_block_offset <= BLOCK - 4):
        ap.error("--in-block-offset must leave room for 4 bytes")

    block = load_block(args)
    original = block[args.in_block_offset:args.in_block_offset + 4]
    if original != args.expect_bytes:
        raise SystemExit(
            f"original bytes {original.hex()} != expected {args.expect_bytes.hex()}")
    if args.expect_block_sha and sha256(block) != args.expect_block_sha.lower():
        raise SystemExit(
            f"block sha256 {sha256(block)} != expected {args.expect_block_sha.lower()}")

    patched = bytearray(block)
    patched[args.in_block_offset:args.in_block_offset + 4] = args.patch_bytes
    patched = bytes(patched)
    if args.expect_patched_sha and sha256(patched) != args.expect_patched_sha.lower():
        raise SystemExit(
            f"patched sha256 {sha256(patched)} != expected {args.expect_patched_sha.lower()}")

    print(f"offset        : 0x{args.in_block_offset:x}")
    print(f"original      : {original.hex()}")
    print(f"patched       : {args.patch_bytes.hex()}")
    print(f"block sha256  : {sha256(block)}")
    print(f"result sha256 : {sha256(patched)}")

    if not args.apply:
        print("dry run; pass --apply to write")
        return
    write_block(args, patched)


if __name__ == "__main__":
    main()
