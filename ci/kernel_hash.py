#!/usr/bin/env python3
"""Hash a Mach-O kernel by segment, separating code from debug metadata.

Full-file SHA-256 is not a reproducibility gate for XNU CI builds: DWARF and
symbol paths can differ between checkouts.  This tool reports:

  full          SHA-256 of the whole file
  core          SHA-256 over every segment except __LINKEDIT (debug/symbols)
  exec-data     SHA-256 over __TEXT_EXEC and the data segments, excluding
                __TEXT (cstrings may embed build paths) and __LINKEDIT

The per-segment hashes make a mismatch diagnosable.  The booted 0085 kernel
was built by the baseline harness; use its exec-data hash as the reference
for a rebuild that must run the same code.

    python3 ci/kernel_hash.py [--expect-exec-data HEX] KERNEL
"""
import hashlib
import struct
import sys

MH_MAGIC_64 = 0xFEEDFACF
LC_SEGMENT_64 = 0x19
EXEC_DATA_SEGMENTS = {
    "__TEXT_EXEC", "__DATA_CONST", "__DATA", "__BOOTDATA",
    "__KLD", "__KLDDATA", "__LASTDATA_CONST", "__LINKINFO",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def parse(path):
    data = open(path, "rb").read()
    if len(data) < 32:
        raise SystemExit("kernel too small")
    magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, res = \
        struct.unpack_from("<IiiIIIII", data, 0)
    if magic != MH_MAGIC_64:
        raise SystemExit("not a little-endian Mach-O 64 kernel")
    off = 32
    segs = []
    for _ in range(ncmds):
        if off + 8 > len(data):
            raise SystemExit("truncated load commands")
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmdsize < 8 or off + cmdsize > len(data):
            raise SystemExit("bad load command size")
        if cmd == LC_SEGMENT_64:
            if cmdsize < 72:
                raise SystemExit("short LC_SEGMENT_64")
            segname = data[off + 8:off + 24].split(b"\0", 1)[0].decode("ascii", "replace")
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, off + 24)
            if fileoff + filesize > len(data):
                raise SystemExit(f"segment {segname} exceeds file")
            body = data[fileoff:fileoff + filesize]
            digest = sha(segname.encode() + struct.pack("<QQQ", vmaddr, vmsize, filesize) + body)
            segs.append({
                "name": segname, "vmaddr": vmaddr, "vmsize": vmsize,
                "fileoff": fileoff, "filesize": filesize, "sha256": digest,
            })
        off += cmdsize
    return data, segs


def combined(segs, names=None):
    h = hashlib.sha256()
    for seg in sorted(segs, key=lambda s: s["name"]):
        if names is not None and seg["name"] not in names:
            continue
        if names is None and seg["name"] == "__LINKEDIT":
            continue
        h.update(seg["name"].encode())
        h.update(b"\0")
        h.update(bytes.fromhex(seg["sha256"]))
    return h.hexdigest()


def main():
    args = sys.argv[1:]
    expect = None
    if args and args[0] == "--expect-exec-data":
        if len(args) < 3:
            raise SystemExit("usage: kernel_hash.py [--expect-exec-data HEX] KERNEL")
        expect = args[1].lower()
        args = args[2:]
    if len(args) != 1:
        raise SystemExit("usage: kernel_hash.py [--expect-exec-data HEX] KERNEL")

    data, segs = parse(args[0])
    full = sha(data)
    core = combined(segs)
    exec_data = combined(segs, EXEC_DATA_SEGMENTS)

    print(f"file           {args[0]}")
    print(f"size           {len(data)}")
    print(f"full_sha256    {full}")
    print(f"core_sha256    {core}")
    print(f"exec_data_sha256 {exec_data}")
    for seg in segs:
        print(f"seg {seg['name']:<16} vm=0x{seg['vmaddr']:x} "
              f"size=0x{seg['vmsize']:x} file=0x{seg['fileoff']:x}+0x{seg['filesize']:x} "
              f"sha256={seg['sha256']}")

    if expect is not None and exec_data != expect:
        print(f"exec-data hash mismatch: expected {expect}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
