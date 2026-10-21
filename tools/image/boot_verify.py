#!/usr/bin/env python3
"""Read back a SWEETXNU-packed boot image and report its payload layout.

    python boot_verify.py <image.img> [<base.img>]

Decompresses the kernel slot, parses the SWEETXNU payload header and prints
every field, so a freshly built image can be diffed against a known-good one
before flashing. Read-only.
"""
import struct
import sys
import zlib

FIELDS = ["virt_base", "entry_va", "adt_off", "adt_size", "kernel_end_va",
          "phys_base", "mem_size"]


def sha(b):
    import hashlib
    return hashlib.sha256(b).hexdigest()


def inspect(path):
    img = open(path, "rb").read()
    page = struct.unpack_from("<I", img, 36)[0]
    ksize = struct.unpack_from("<I", img, 8)[0]
    raw = zlib.decompress(bytes(img[page:page + ksize]), 16 + zlib.MAX_WBITS)
    hdr = raw[0x14000:0x14000 + 0x2A8]
    if hdr[:8] != b"SWEETXNU":
        raise SystemExit(f"{path}: no SWEETXNU magic at 0x14000")
    ver, nseg = struct.unpack_from("<II", hdr, 8)
    vals = struct.unpack_from("<7Q", hdr, 0x10)
    info = dict(zip(FIELDS, vals))
    info["ver"], info["nseg"] = ver, nseg
    info["cmdline"] = hdr[0x48:0x48 + 608].split(b"\0")[0].decode()
    adt = raw[0x14000 + info["adt_off"]:0x14000 + info["adt_off"] + info["adt_size"]]
    info["adt_sha256"] = sha(adt)
    info["img_sha256"] = sha(img)
    info["raw_len"] = len(raw)
    info["page"], info["kernel_size"] = page, ksize
    info["adt"] = adt
    info["raw"] = raw
    info["img"] = img
    return info


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    info = inspect(sys.argv[1])
    print(f"image        : {sys.argv[1]}")
    print(f"image sha256 : {info['img_sha256']}")
    print(f"page/kernel  : {info['page']} / {info['kernel_size']}")
    print(f"raw len      : {info['raw_len']}")
    print(f"SWEETXNU     : version={info['ver']} nsegs={info['nseg']}")
    for k in FIELDS:
        print(f"  {k:16s}= 0x{info[k]:x}")
    print(f"  adt_sha256    = {info['adt_sha256']}")
    print(f"cmdline      : {info['cmdline']}")
    for s in (b"SWEET IOX: doServiceMatch", b"SWEET IOX: addInstance",
              b"SWEET IOX: class-match", b"SWEET IOX: addMatchingNotification",
              b"SWEET IOX: copyNotifiers", b"SWEET IOX:   cand"):
        n = info["raw"].count(s)
        if n:
            print(f"trace string : {s.decode():42s} x{n}")
    if len(sys.argv) > 2:
        base = open(sys.argv[2], "rb").read()
        page, ksize = info["page"], info["kernel_size"]
        print(f"tail == base (outside kernel slot): "
              f"{info['img'][page + ksize:] == base[page + ksize:]}")


if __name__ == "__main__":
    main()
