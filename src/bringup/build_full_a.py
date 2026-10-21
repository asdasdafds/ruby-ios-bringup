#!/usr/bin/env python3
"""Pack a freestanding payload into an Android boot image's kernel slot.

The base image and payload are supplied by the caller; no device paths are
hard-coded. The kernel slot keeps its original compressed size so the boot
image header stays unchanged.
"""
import argparse
import struct
import zlib


def parse_int(value):
    return int(value, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="stock/Magisk boot image")
    ap.add_argument("--payload", required=True, help="raw AArch64 payload")
    ap.add_argument("--out", required=True, help="output boot image")
    ap.add_argument("--payload-off", type=parse_int, default=0x1000)
    args = ap.parse_args()

    img = bytearray(open(args.base, "rb").read())
    payload = open(args.payload, "rb").read()

    page = struct.unpack_from("<I", img, 36)[0]
    kernel_size = struct.unpack_from("<I", img, 8)[0]
    if img[:8] != b"ANDROID!":
        raise SystemExit("not a boot image")

    comp = bytes(img[page:page + kernel_size])
    raw = zlib.decompress(comp, 16 + zlib.MAX_WBITS)
    if raw[0x38:0x3c] != b"ARM\x64":
        raise SystemExit(f"ARM64 header magic not found: {raw[0x38:0x3c].hex()}")

    mod = bytearray(raw)
    branch = 0x14000000 | ((args.payload_off - 0) // 4)
    mod[0:4] = struct.pack("<I", branch)
    mod[args.payload_off:args.payload_off + len(payload)] = payload

    co = zlib.compressobj(9, zlib.DEFLATED, -15)
    deflate = co.compress(bytes(mod)) + co.flush()
    extra_len = kernel_size - 20 - len(deflate)
    if extra_len < 0 or extra_len > 0xFFFF:
        raise SystemExit(f"cannot fit gzip: deflate={len(deflate)} extra={extra_len}")

    header = b"\x1f\x8b\x08\x04" + b"\x00" * 4 + b"\x02\x03" + struct.pack("<H", extra_len)
    footer = struct.pack("<II", zlib.crc32(mod) & 0xFFFFFFFF, len(mod) & 0xFFFFFFFF)
    gz = header + b"\x00" * extra_len + deflate + footer
    if len(gz) != kernel_size:
        raise SystemExit(f"gzip size {len(gz)} != {kernel_size}")

    img[page:page + kernel_size] = gz
    open(args.out, "wb").write(img)

    check = zlib.decompress(gz, 16 + zlib.MAX_WBITS)
    print(f"page={page} kernel_size={kernel_size} gzip={len(gz)} extra={extra_len}")
    print(f"raw={len(mod)} branch={branch:08x} entry={args.payload_off:#x} payload={len(payload)}")
    print(f"header_magic={check[0x38:0x3c].hex()} head={check[:8].hex()} "
          f"payload_head={check[args.payload_off:args.payload_off + 8].hex()}")
    print(f"out={args.out} size={len(img)}")


if __name__ == "__main__":
    main()
