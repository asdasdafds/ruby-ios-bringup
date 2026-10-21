#!/usr/bin/env python3
"""Read-only scanner for Apple's split dyld shared cache.

Given a cache directory (the one containing dyld_shared_cache_arm64 and its
.01..NN subcaches), an image name and one or more virtual addresses, print
which image contains each address and, for subcaches, the file offset and the
first 0x80 bytes at that VA.

No files are written. This is the same parser used during the display and
MobileGestalt work, with the paths and image name moved to arguments.

Example:
  python dyld_scan.py --cache-dir /path/to/com.apple.dyld \
      --image-name InstalledContentLibrary --va 0x1b11b9f0c
"""
import argparse
import os
import struct
import sys


def parse_mappings(path):
    with open(path, "rb") as f:
        h = f.read(0x100)
        magic = h[:16].rstrip(b"\0").decode("latin1")
        moff, mcnt = struct.unpack_from("<II", h, 0x10)
        f.seek(moff)
        out = []
        for _ in range(mcnt):
            addr, size, foff, maxp, initp = struct.unpack("<QQQII", f.read(32))
            out.append((addr, size, foff, maxp, initp))
        return magic, out


def parse_images(path):
    with open(path, "rb") as f:
        h = f.read(0x400)
        off, cnt = struct.unpack_from("<QQ", h, 0x88)
        f.seek(off)
        arr = f.read(cnt * 32)
        out = []
        for i in range(cnt):
            entry = arr[i * 32:(i + 1) * 32]
            load, size, pathoff = struct.unpack_from("<QII", entry, 16)
            f.seek(pathoff)
            name = b""
            while True:
                c = f.read(1)
                if not c or c == b"\0":
                    break
                name += c
            out.append((load, load + size, name.decode("utf-8", "replace")))
        return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cache-dir", required=True)
    ap.add_argument("--main", default="dyld_shared_cache_arm64")
    ap.add_argument("--image-name", default="InstalledContentLibrary")
    ap.add_argument("--va", action="append", type=lambda v: int(v, 0), default=[])
    args = ap.parse_args()

    main_path = os.path.join(args.cache_dir, args.main)
    if not os.path.isfile(main_path):
        raise SystemExit(f"missing main cache: {main_path}")

    images = parse_images(main_path)
    hits = [(lo, hi, name) for lo, hi, name in images if args.image_name in name]
    for lo, hi, name in hits:
        print(f"image 0x{lo:x}-0x{hi:x} {name}")

    if not args.va:
        return

    subcaches = []
    for name in sorted(os.listdir(args.cache_dir)):
        if not name.startswith(args.main + ".") or name.endswith(".symbols"):
            continue
        path = os.path.join(args.cache_dir, name)
        if not os.path.isfile(path):
            continue
        try:
            magic, maps = parse_mappings(path)
        except Exception as exc:
            print(f"{name}: parse error: {exc}", file=sys.stderr)
            continue
        subcaches.append((name, path, maps))

    for va in args.va:
        print(f"=== VA 0x{va:x}")
        for lo, hi, name in images:
            if lo <= va < hi:
                print(f"image {name} +0x{va - lo:x}")
                break
        for name, path, maps in subcaches:
            for addr, size, foff, _maxp, _initp in maps:
                if addr <= va < addr + size:
                    off = foff + (va - addr)
                    with open(path, "rb") as f:
                        f.seek(off)
                        data = f.read(0x80)
                    print(f"subcache {name} map=0x{addr:x}+0x{size:x} fileoff=0x{off:x}")
                    print(f"hex {data.hex()}")
                    print("asc " + "".join(chr(c) if 32 <= c < 127 else "." for c in data))
                    break


if __name__ == "__main__":
    main()
