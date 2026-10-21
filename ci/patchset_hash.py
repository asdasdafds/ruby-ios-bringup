#!/usr/bin/env python3
"""Canonical hash of an XNU patchset.

The hash is independent of the checkout path and line endings: every .patch
and .sh file is normalized to LF, hashed, and the sorted "<sha>  <name>" lines
are hashed again.

    python patchset_hash.py <patches-dir>

Prints the hash on stdout and the file count on stderr.
"""
import hashlib
import os
import sys


def normalize(data):
    return data.replace(b"\r\n", b"\n")


def patchset_hash(root):
    lines = []
    for name in sorted(os.listdir(root)):
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        if not (name.endswith(".patch") or name.endswith(".sh")):
            continue
        digest = hashlib.sha256(normalize(open(path, "rb").read())).hexdigest()
        lines.append(f"{digest}  {name}\n")
    return hashlib.sha256("".join(lines).encode()).hexdigest(), len(lines)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    digest, count = patchset_hash(root)
    print(digest)
    print(f"files={count}", file=sys.stderr)


if __name__ == "__main__":
    main()
