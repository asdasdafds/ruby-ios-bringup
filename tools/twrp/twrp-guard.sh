#!/bin/sh
# Refuse to run a raw block tool outside TWRP.
#
#   twrp-guard.sh ./blkdiff /dev/block/by-name/iosroot crc.bin diff.bin idx 16384
#
# The guard is intentionally small: it checks ro.twrp.version, prints it, and
# then execs the requested command. It does not validate the target device.
set -eu

if [ "$#" -lt 1 ]; then
    echo "usage: twrp-guard.sh CMD [ARG...]" >&2
    exit 2
fi

if ! command -v getprop >/dev/null 2>&1; then
    echo "twrp-guard: getprop not found; refusing to run" >&2
    exit 2
fi

ver="$(getprop ro.twrp.version 2>/dev/null || true)"
if [ -z "$ver" ]; then
    echo "twrp-guard: not running in TWRP (ro.twrp.version is empty)" >&2
    exit 2
fi

echo "twrp-guard: TWRP $ver" >&2
exec "$@"
