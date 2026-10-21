#!/usr/bin/env python3
"""Pack the UFS probe shim into a boot image with a size-preserving kernel slot.

The filler is deterministic (seeded PRNG) so repeated builds with the same
inputs produce the same image.
"""
import argparse
import random
import struct
import zlib


def parse_int(value):
    return int(value, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="stock/Magisk boot image")
    ap.add_argument("--shim", required=True, help="raw AArch64 UFS probe shim")
    ap.add_argument("--out", required=True, help="output boot image")
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    img = bytearray(open(args.base, "rb").read())
    shim = open(args.shim, "rb").read()

    page = struct.unpack_from("<I", img, 36)[0]
    kernel_size = struct.unpack_from("<I", img, 8)[0]
    raw = zlib.decompress(bytes(img[page:page + kernel_size]), 16 + zlib.MAX_WBITS)
    target = kernel_size - 20
    max_fill = len(raw) - len(shim)
    filler = target - 65536
    buf = None
    deflate = b""
    extra = -1
    for _ in range(16):
        if filler < 0 or filler > max_fill:
            raise SystemExit(f"filler {filler} out of range (max {max_fill})")
        buf = bytearray(len(raw))
        buf[:len(shim)] = shim
        buf[len(shim):len(shim) + filler] = random.Random(args.seed).randbytes(filler)
        co = zlib.compressobj(9, zlib.DEFLATED, -15)
        deflate = co.compress(bytes(buf)) + co.flush()
        extra = target - len(deflate)
        if 0 <= extra <= 0xFFFF:
            break
        if extra < 0:
            filler -= (-extra) + 4096
        else:
            filler += (extra - 0xFFFF) + 4096
    else:
        raise SystemExit(f"cannot fit: deflate={len(deflate)} target={target}")

    header = b"\x1f\x8b\x08\x04" + b"\x00" * 4 + b"\x02\x03" + struct.pack("<H", extra)
    footer = struct.pack("<II", zlib.crc32(buf) & 0xFFFFFFFF, len(buf) & 0xFFFFFFFF)
    gz = header + b"\x00" * extra + deflate + footer
    if len(gz) != kernel_size:
        raise SystemExit("gzip size mismatch")

    img[page:page + kernel_size] = gz
    open(args.out, "wb").write(img)
    print(f"shim={len(shim)} filler={filler} deflate={len(deflate)} "
          f"extra={extra} out={args.out}")


if __name__ == "__main__":
    main()
