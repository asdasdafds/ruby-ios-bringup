#!/usr/bin/env python3
"""Verify the success-bundle artifact hashes. Read-only, no device access.

The device baseline (GPT/boot/vbmeta) is deliberately not hashed here; only
the artifacts that form the success contract are checked.

    python scripts/check-bundle.py \
        --kernel <kernel> --boot <boot.img> --root <root.img> \
        --icl-orig <orig.bin> --icl-patched <patched.bin> \
        --cmdline <cmdline.txt>

Every argument is optional; missing files are reported as skipped. The
expected values come from config/0084.env.
"""
import argparse
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, "config", "0084.env")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_env(path):
    values = {}
    for line in open(path, "r", encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def check(label, path, expected, required=False):
    if not path:
        if required:
            print(f"FAIL  {label}: not provided (strict)")
            return False
        print(f"skip  {label}: not provided")
        return True
    if not os.path.isfile(path):
        print(f"FAIL  {label}: missing {path}")
        return False
    actual = sha256(path)
    if actual != expected:
        print(f"FAIL  {label}: {actual} != {expected}")
        return False
    print(f"ok    {label}: {actual}")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ("kernel", "boot", "root", "icl-orig", "icl-patched", "cmdline",
                 "shim", "payload", "base-boot"):
        ap.add_argument("--" + name)
    ap.add_argument("--strict", action="store_true",
                    help="require kernel/boot/shim/payload/base-boot/cmdline")
    ap.add_argument("--require-root", action="store_true",
                    help="also require root and ICL blocks (reads the root, no writes)")
    args = ap.parse_args()

    pins = read_env(CONFIG)
    ok = True
    required = args.strict
    ok &= check("kernel", args.kernel, pins["KERNEL_SHA256"], required)
    ok &= check("boot image", args.boot, pins["IMAGE_SHA256"], required)
    ok &= check("shim", args.shim, pins["SHIM_SHA256"], required)
    ok &= check("payload", args.payload, pins["PAYLOAD_SHA256"], required)
    ok &= check("base boot", args.base_boot, pins["BASE_BOOT_SHA256"], required)
    ok &= check("root image", args.root, pins["ROOT_SHA256"], args.require_root)
    ok &= check("ICL original block", args.icl_orig, pins["ICL_ORIG_SHA256"], args.require_root)
    ok &= check("ICL patched block", args.icl_patched, pins["ICL_PATCHED_SHA256"], args.require_root)

    cmdline_ref = os.path.join(ROOT, pins["BOOT_CMDLINE"])
    if args.cmdline:
        if not os.path.isfile(args.cmdline):
            print(f"FAIL  cmdline: missing {args.cmdline}")
            ok = False
        else:
            want = open(cmdline_ref, "r", encoding="utf-8").read().strip()
            got = open(args.cmdline, "r", encoding="utf-8").read().strip()
            if got != want:
                print("FAIL  cmdline: does not match config/0084-boot.cmdline")
                ok = False
            else:
                print("ok    cmdline")
    else:
        if required:
            print("FAIL  cmdline: not provided (strict)")
            ok = False
        else:
            print("skip  cmdline: not provided")

    print("note  : for a rebuilt image use tools/image/boot_layout.py; its SHA differs by design")
    print("PASS" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
