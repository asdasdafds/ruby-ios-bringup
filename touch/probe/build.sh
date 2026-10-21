#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)

SHIM_DIR=${SHIM_DIR:?set SHIM_DIR to the ruby-shim directory}
BASE_IMG=${BASE_IMG:?set BASE_IMG to the base boot image}
OUTDIR=${OUTDIR:-out}
SRC=${SRC:-$HERE/touch_probe.c}
NAME=${NAME:-touch-p0}
SEED=${SEED:-112}
BASE_SHA=9f867d75ba10367394ded7842e99220c3b4265ef78ed4d3ab1aa2a0a2932a5c8

CLANG=${CLANG:-clang}
LDLLD=${LDLLD:-ld.lld}
OBJCOPY=${OBJCOPY:-llvm-objcopy}

for f in "$SHIM_DIR/start.S" "$SHIM_DIR/console.c" "$SHIM_DIR/console.h" \
         "$SHIM_DIR/font.h" "$SHIM_DIR/shim.ld" "$BASE_IMG"; do
    [ -f "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

sum=$(sha256sum "$BASE_IMG" | awk '{print $1}')
if [ "$sum" != "$BASE_SHA" ]; then
    echo "base image hash mismatch" >&2
    echo "expected $BASE_SHA" >&2
    echo "actual   $sum" >&2
    exit 1
fi

mkdir -p "$OUTDIR"
# MMU is off in the probe: wide unaligned loads/stores fault as Device memory.
# -mstrict-align alone did not stop LLVM from emitting `ldur s0,[x0,#2]`;
# disable vectorization so byte buffers stay byte accesses.
CFLAGS="--target=aarch64-none-elf -march=armv8-a -mstrict-align -ffreestanding \
-fno-vectorize -fno-slp-vectorize \
-fno-builtin -fno-stack-protector -fno-pic -fno-pie \
-fno-asynchronous-unwind-tables -fno-unwind-tables -O2"

"$CLANG" $CFLAGS -c "$SHIM_DIR/start.S" -o "$OUTDIR/start.o"
"$CLANG" $CFLAGS -I"$SHIM_DIR" -c "$SHIM_DIR/console.c" -o "$OUTDIR/console.o"
"$CLANG" $CFLAGS -I"$SHIM_DIR" -c "$SRC" -o "$OUTDIR/$NAME.o"
"$LDLLD" -T "$SHIM_DIR/shim.ld" -o "$OUTDIR/$NAME.elf" \
    "$OUTDIR/start.o" "$OUTDIR/console.o" "$OUTDIR/$NAME.o"
"$OBJCOPY" -O binary "$OUTDIR/$NAME.elf" "$OUTDIR/$NAME.Image"

[ "$(od -An -tx1 -j56 -N4 "$OUTDIR/$NAME.Image" | tr -d ' \n')" = "41524d64" ] || {
    echo "bad ARM64 magic at 0x38" >&2
    exit 1
}

python "$ROOT/src/ufs-probe/build_ufs_full.py" \
    --base "$BASE_IMG" --shim "$OUTDIR/$NAME.Image" \
    --out "$OUTDIR/$NAME.img" --seed "$SEED"

echo "built: $OUTDIR/$NAME.img"
sha256sum "$OUTDIR/$NAME.img" "$OUTDIR/$NAME.Image"
echo "NOT flashed. Flash boot_a only after explicit confirmation."
