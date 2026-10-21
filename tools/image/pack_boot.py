#!/usr/bin/env python3
"""Pack a shim + SWEETXNU payload into a 128 MiB Android boot image.

The filler is deterministic (SHA-256 counter stream), so the output is stable
for a given base/shim/payload/seed. The original 0084 image used os.urandom
filler, so it cannot be byte-reproduced; use boot_layout.py to compare layout.

The base image and the shim/ADT/payload are supplied by the caller; none of
them are in this repository.
"""
import argparse
import hashlib
import struct
import sys
import zlib


def parse_int(value):
    return int(value, 0)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def filler_bytes(seed, size):
    out = bytearray()
    counter = 0
    while len(out) < size:
        out += hashlib.sha256(struct.pack("<QQ", seed, counter)).digest()
        counter += 1
    return bytes(out[:size])


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", required=True, help="stock/Magisk 128 MiB boot image")
    ap.add_argument("--shim", required=True, help="raw loader shim (Image)")
    ap.add_argument("--payload", required=True, help="SWEETXNU payload")
    ap.add_argument("--out", required=True)
    ap.add_argument("--payload-off", type=parse_int, default=0x14000)
    ap.add_argument("--seed", type=parse_int, default=0x84)
    ap.add_argument("--expect-base-sha")
    ap.add_argument("--expect-shim-sha")
    ap.add_argument("--expect-payload-sha")
    ap.add_argument("--expect-out-sha")
    args = ap.parse_args()

    base = bytearray(open(args.base, "rb").read())
    shim = open(args.shim, "rb").read()
    payload = open(args.payload, "rb").read()

    if base[:8] != b"ANDROID!":
        raise SystemExit("not an Android boot image")
    page = struct.unpack_from("<I", base, 36)[0]
    ksize = struct.unpack_from("<I", base, 8)[0]
    raw = zlib.decompress(bytes(base[page:page + ksize]), 16 + zlib.MAX_WBITS)

    if args.expect_base_sha and sha256(base) != args.expect_base_sha.lower():
        raise SystemExit(f"base sha256 {sha256(base)} != expected")
    if args.expect_shim_sha and sha256(shim) != args.expect_shim_sha.lower():
        raise SystemExit(f"shim sha256 {sha256(shim)} != expected")
    if args.expect_payload_sha and sha256(payload) != args.expect_payload_sha.lower():
        raise SystemExit(f"payload sha256 {sha256(payload)} != expected")

    off = args.payload_off
    if len(shim) > off:
        raise SystemExit(f"shim {len(shim)} > payload offset {off}")
    if off + len(payload) > len(raw):
        raise SystemExit(f"payload does not fit raw ({off}+{len(payload)} > {len(raw)})")

    target = ksize - 20
    max_fill = len(raw) - off - len(payload)
    filler_all = filler_bytes(args.seed, max_fill)
    filler = target - 65536
    deflate = b""
    extra = -1
    buf = None
    for _ in range(64):
        if filler < 0 or filler > max_fill:
            raise SystemExit(f"filler {filler} out of range (max {max_fill})")
        buf = bytearray(len(raw))
        buf[:len(shim)] = shim
        buf[off:off + len(payload)] = payload
        buf[off + len(payload):off + len(payload) + filler] = filler_all[:filler]
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
    if len(gz) != ksize:
        raise SystemExit(f"gzip size {len(gz)} != {ksize}")

    out = bytearray(base)
    out[page:page + ksize] = gz

    check = zlib.decompress(bytes(out[page:page + ksize]), 16 + zlib.MAX_WBITS)
    if check[:len(shim)] != shim:
        raise SystemExit("readback shim mismatch")
    if check[off:off + len(payload)] != payload:
        raise SystemExit("readback payload mismatch")
    if out[page + ksize:] != base[page + ksize:]:
        raise SystemExit("tail changed")

    out_sha = sha256(out)
    if args.expect_out_sha and out_sha != args.expect_out_sha.lower():
        raise SystemExit(f"output sha256 {out_sha} != expected (environment-dependent)")
    open(args.out, "wb").write(out)
    print(f"base      : {args.base}")
    print(f"shim      : {len(shim)} B {sha256(shim)}")
    print(f"payload   : {len(payload)} B {sha256(payload)}")
    print(f"page/ksize: {page} / {ksize}")
    print(f"raw       : {len(raw)}")
    print(f"filler    : {filler} (seed {args.seed:#x})")
    print(f"out       : {args.out} {len(out)} B")
    print(f"out sha256: {out_sha}")
    print("note      : deterministic filler; image SHA differs from the original 0084 image")


if __name__ == "__main__":
    main()
