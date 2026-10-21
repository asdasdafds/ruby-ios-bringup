#!/usr/bin/env python3
"""Pack a stage shim into an Android boot image's kernel slot.

Same slot-preserving approach as build_full_a.py, with the entry branch and
payload offset kept as explicit arguments.
"""
import argparse
import struct
import zlib


def parse_int(value):
    return int(value, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="stock/Magisk boot image")
    ap.add_argument("--payload", required=True, help="raw AArch64 shim")
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
        raise SystemExit("ARM64 header magic missing")

    mod = bytearray(raw)
    mod[0:4] = struct.pack("<I", 0x14000000 | (args.payload_off // 4))
    mod[args.payload_off:args.payload_off + len(payload)] = payload

    co = zlib.compressobj(9, zlib.DEFLATED, -15)
    deflate = co.compress(bytes(mod)) + co.flush()
    extra_len = kernel_size - 20 - len(deflate)
    if extra_len < 0 or extra_len > 0xFFFF:
        raise SystemExit(f"gzip fit failed: deflate={len(deflate)} extra={extra_len}")

    header = b"\x1f\x8b\x08\x04" + b"\x00" * 4 + b"\x02\x03" + struct.pack("<H", extra_len)
    footer = struct.pack("<II", zlib.crc32(mod) & 0xFFFFFFFF, len(mod) & 0xFFFFFFFF)
    gz = header + b"\x00" * extra_len + deflate + footer
    if len(gz) != kernel_size:
        raise SystemExit(f"gzip size {len(gz)} != {kernel_size}")

    img[page:page + kernel_size] = gz
    open(args.out, "wb").write(img)

    check = zlib.decompress(gz, 16 + zlib.MAX_WBITS)
    print(f"payload={len(payload)} gzip={len(gz)} extra={extra_len} raw={len(mod)}")
    print(f"entry_branch={mod[0:4].hex()} "
          f"payload_head={check[args.payload_off:args.payload_off + 16].hex()}")
    print(f"arm64_magic={check[0x38:0x3c].hex()} out={args.out} size={len(img)}")


if __name__ == "__main__":
    main()
