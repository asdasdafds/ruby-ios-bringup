#!/usr/bin/env python3
"""Normalize .patch and .sh files in a directory to LF.

Used by prepare-baseline.sh so the patchset hash and the applied text are the
same on Windows and macOS. Only the target directory is modified.
"""
import os
import sys


def main():
    root = sys.argv[1]
    changed = 0
    for name in sorted(os.listdir(root)):
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        if not (name.endswith(".patch") or name.endswith(".sh")):
            continue
        data = open(path, "rb").read()
        new = data.replace(b"\r\n", b"\n")
        if new != data:
            open(path, "wb").write(new)
            changed += 1
    print(f"normalized {changed} file(s) in {root}")


if __name__ == "__main__":
    main()
