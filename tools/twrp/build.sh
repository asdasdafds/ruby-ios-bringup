#!/bin/sh
# Cross-compile the freestanding aarch64 TWRP tools.
#
# Set CC to a clang that can target aarch64-linux-gnu and finds lld, for
# example the DevEco clang used during the original bring-up.
set -eu
cd "$(dirname "$0")"

CC="${CC:-clang}"
for src in blkdiff blkpatch; do
    echo "building $src"
    "$CC" --target=aarch64-linux-gnu -O2 -nostdlib -static -no-pie \
        -fuse-ld=lld -Wl,-e,_start "$src.c" -o "$src"
done

ls -l blkdiff blkpatch
file blkdiff blkpatch 2>/dev/null || true
