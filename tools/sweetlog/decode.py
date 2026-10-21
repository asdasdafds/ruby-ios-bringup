#!/usr/bin/env python3
"""Decode a SWEETLOG ring area.

The area is the last 1025 blocks of 4096 bytes: a 4 KiB header followed by a
4 MiB ring. The header is ``SWEETLOG``, u32 version, u32 ring size, u64 total
bytes and u64 flush sequence. Byte n of the console stream is at ring offset
n % size. A wrapped ring loses the oldest bytes; this tool reports that.

  decode.py AREA.bin [OUT.txt]

The tool is host-only. It does not touch a device.
"""
import struct
import sys


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit("usage: decode.py AREA.bin [OUT.txt]")
    data = open(sys.argv[1], "rb").read()
    magic, version, size, total, seq = struct.unpack_from("<8sIIQQ", data, 0)
    if magic != b"SWEETLOG":
        sys.exit("no SWEETLOG header (log never flushed, or wrong area)")
    ring = data[4096:4096 + size]
    if len(ring) != size:
        sys.exit("short ring: %d of %d bytes" % (len(ring), size))
    if total <= size:
        text = ring[:total]
    else:
        start = total % size
        text = ring[start:] + ring[:start]
    out = text.decode("utf-8", "replace").replace("\r", "")
    wrapped = " (wrapped: oldest bytes lost)" if total > size else ""
    print("# version %d, ring %d, total %d bytes, flush #%d%s"
          % (version, size, total, seq, wrapped), file=sys.stderr)
    if len(sys.argv) == 3:
        open(sys.argv[2], "w", encoding="utf-8").write(out)
    else:
        sys.stdout.write(out)


if __name__ == "__main__":
    main()
