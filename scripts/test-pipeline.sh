#!/bin/sh
# Host-only end-to-end test: payload -> deterministic boot image -> layout.
#
# No device, no root image. Inputs are external (<1GB):
#   --base stock/Magisk boot image
#   --kernel kernel.development.sweet
#   --adt ADT
#   --shim loader Image
#   --golden golden 0084 boot image
#
#   sh scripts/test-pipeline.sh --base ... --kernel ... --adt ... \
#     --shim ... --golden ...
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PYTHON=${PYTHON:-python3}
BASE=
KERNEL=
ADT=
SHIM=
GOLDEN=
EXPECT_PAYLOAD=50701e819368239252527cc61e9bb67d4d7387798272d781f0a72470751de178
EXPECT_BASE=9f867d75ba10367394ded7842e99220c3b4265ef78ed4d3ab1aa2a0a2932a5c8
EXPECT_KERNEL=b38fca5f38cfb721e21228eac4dd4b355000d1b850bdf204afba9ff8b8d1afcd
EXPECT_ADT=c778d4b5a559ad7c4e8f54113cc8b4e7d91d34082efc91acb02e1fcce0099720
EXPECT_SHIM=63cd3f3529e37d169fb56b1f122fffbb295831c3fe05b7f88a756e8966676efc

while [ "$#" -gt 0 ]; do
    case "$1" in
        --base) BASE=$2; shift 2 ;;
        --kernel) KERNEL=$2; shift 2 ;;
        --adt) ADT=$2; shift 2 ;;
        --shim) SHIM=$2; shift 2 ;;
        --golden) GOLDEN=$2; shift 2 ;;
        --expect-payload-sha) EXPECT_PAYLOAD=$2; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

[ -n "$BASE" ] && [ -n "$KERNEL" ] && [ -n "$ADT" ] && [ -n "$SHIM" ] || {
    echo "usage: test-pipeline.sh --base B --kernel K --adt A --shim S [--golden G]" >&2
    exit 2
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
PAYLOAD="$WORK/payload.bin"
BOOT="$WORK/boot.img"

echo "== payload =="
PYTHON="$PYTHON" sh "$ROOT/scripts/build-payload.sh" \
    --kernel "$KERNEL" --adt "$ADT" \
    --cmdline-file "$ROOT/config/0084-boot.cmdline" \
    --out "$PAYLOAD" --expect-sha "$EXPECT_PAYLOAD" \
    --expect-kernel-sha "$EXPECT_KERNEL" --expect-adt-sha "$EXPECT_ADT"

echo "== pack =="
"$PYTHON" "$ROOT/tools/image/pack_boot.py" \
    --base "$BASE" --shim "$SHIM" --payload "$PAYLOAD" --out "$BOOT" \
    --expect-base-sha "$EXPECT_BASE" \
    --expect-shim-sha "$EXPECT_SHIM" \
    --expect-payload-sha "$EXPECT_PAYLOAD"

echo "== layout =="
if [ -n "$GOLDEN" ]; then
    "$PYTHON" "$ROOT/tools/image/boot_layout.py" \
        --image "$BOOT" --golden "$GOLDEN" --config "$ROOT/config/0084.env" \
        --payload "$PAYLOAD" --base "$BASE"
else
    "$PYTHON" "$ROOT/tools/image/boot_layout.py" \
        --image "$BOOT" --config "$ROOT/config/0084.env" \
        --payload "$PAYLOAD" --base "$BASE"
fi

echo "PASS: payload + boot layout"
