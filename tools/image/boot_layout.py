#!/usr/bin/env python3
"""Compare a rebuilt boot image by layout.

Two modes:

  --golden <0084.img>   compare against a golden image (optional)
  --config <0084.env>   compare against pinned header/hash values, no golden

Both can be used together. The image SHA is expected to differ because the
original used os.urandom filler; this tool compares the parts that matter:
SWEETXNU fields, ADT, cmdline, shim/payload ranges and the tail.
"""
import argparse
import hashlib
import os
import struct
import sys
import zlib

MAGIC_OFF = 0x14000
CMDLINE_OFF = 0x48
CMDLINE_LEN = 608


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_env(path):
    values = {}
    for line in open(path, "r", encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def load(path):
    data = open(path, "rb").read()
    if data[:8] != b"ANDROID!":
        raise SystemExit(f"{path}: not an Android boot image")
    page = struct.unpack_from("<I", data, 36)[0]
    ksize = struct.unpack_from("<I", data, 8)[0]
    raw = zlib.decompress(bytes(data[page:page + ksize]), 16 + zlib.MAX_WBITS)
    hdr = raw[MAGIC_OFF:MAGIC_OFF + 0x2A8]
    if hdr[:8] != b"SWEETXNU":
        raise SystemExit(f"{path}: no SWEETXNU header")
    ver, nseg = struct.unpack_from("<II", hdr, 8)
    vals = struct.unpack_from("<7Q", hdr, 0x10)
    cmdline = hdr[CMDLINE_OFF:CMDLINE_OFF + CMDLINE_LEN].split(b"\0")[0].decode()
    adt_off, adt_size = vals[2], vals[3]
    adt = raw[MAGIC_OFF + adt_off:MAGIC_OFF + adt_off + adt_size]
    return dict(path=path, data=data, page=page, ksize=ksize, raw=raw,
                ver=ver, nseg=nseg, vals=vals, cmdline=cmdline,
                adt_sha=sha256(adt), img_sha=sha256(data))


def tail(rec):
    return rec["data"][rec["page"] + rec["ksize"]:]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True, help="rebuilt image")
    ap.add_argument("--golden", help="golden 0084 image (optional)")
    ap.add_argument("--config", help="config/0084.env for pinned layout")
    ap.add_argument("--payload", required=True, help="payload used for the image")
    ap.add_argument("--payload-off", type=lambda v: int(v, 0), default=0x14000)
    ap.add_argument("--base", help="optional base image for tail comparison")
    args = ap.parse_args()

    if not args.golden and not args.config:
        ap.error("pass --golden and/or --config")

    a = load(args.image)
    b = load(args.golden) if args.golden else None
    payload = open(args.payload, "rb").read()
    off = args.payload_off
    base = open(args.base, "rb").read() if args.base else None
    cfg = read_env(args.config) if args.config else None

    checks = []
    if b is not None:
        checks += [
            ("golden page", a["page"] == b["page"]),
            ("golden kernel slot", a["ksize"] == b["ksize"]),
            ("golden raw length", len(a["raw"]) == len(b["raw"])),
            ("golden SWEETXNU version/nseg", (a["ver"], a["nseg"]) == (b["ver"], b["nseg"])),
            ("golden header fields", a["vals"] == b["vals"]),
            ("golden ADT sha256", a["adt_sha"] == b["adt_sha"]),
            ("golden cmdline", a["cmdline"] == b["cmdline"]),
            ("golden shim range", a["raw"][:off] == b["raw"][:off]),
            ("golden payload range", a["raw"][off:off + len(payload)] ==
             b["raw"][off:off + len(payload)]),
            ("golden payload input", a["raw"][off:off + len(payload)] == payload),
        ]
        if base is not None:
            checks.append(("golden tail == base", tail(a) == base[a["page"] + a["ksize"]:] and
                           tail(b) == base[b["page"] + b["ksize"]:]))
        else:
            checks.append(("golden tail equal", tail(a) == tail(b)))

    if cfg is not None:
        root = os.path.dirname(os.path.dirname(os.path.abspath(args.config)))
        cmdline_path = os.path.join(root, cfg["BOOT_CMDLINE"])
        want_cmd = open(cmdline_path, "r", encoding="utf-8").read().strip()
        expected_vals = (
            int(cfg["SWEETXNU_VIRT_BASE"], 0),
            int(cfg["SWEETXNU_ENTRY_VA"], 0),
            int(cfg["SWEETXNU_ADT_OFF"], 0),
            int(cfg["SWEETXNU_ADT_SIZE"], 0),
            int(cfg["SWEETXNU_KERNEL_END_VA"], 0),
            int(cfg["SWEETXNU_PHYS_BASE"], 0),
            int(cfg["SWEETXNU_MEM_SIZE"], 0),
        )
        checks += [
            ("config page", a["page"] == int(cfg["PAGE"])),
            ("config kernel slot", a["ksize"] == int(cfg["KERNEL_SIZE"])),
            ("config raw length", len(a["raw"]) == int(cfg["RAW_LEN"])),
            ("config SWEETXNU version/nseg",
             (a["ver"], a["nseg"]) == (int(cfg["SWEETXNU_VERSION"]), int(cfg["SWEETXNU_NSEGS"]))),
            ("config header fields", a["vals"] == expected_vals),
            ("config ADT sha256", a["adt_sha"] == cfg["ADT_SHA256"].lower()),
            ("config cmdline", a["cmdline"] == want_cmd),
            ("config shim sha256",
             sha256(a["raw"][:int(cfg["SHIM_SIZE"])]) == cfg["SHIM_SHA256"].lower()),
            ("config payload sha256", sha256(payload) == cfg["PAYLOAD_SHA256"].lower()),
            ("config payload range", a["raw"][off:off + len(payload)] == payload),
        ]
        if base is not None:
            checks.append(("config tail == base", tail(a) == base[a["page"] + a["ksize"]:]))

    print(f"image   : {a['img_sha']}")
    if b is not None:
        print(f"golden  : {b['img_sha']}")
    failed = 0
    for name, ok in checks:
        print(f"{'ok  ' if ok else 'FAIL'} {name}")
        failed += 0 if ok else 1
    if b is not None and a["img_sha"] != b["img_sha"]:
        print("note    : image bytes differ (filler); layout checks are the gate")
    print("PASS" if failed == 0 else f"FAILED ({failed})")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
