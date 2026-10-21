#!/bin/sh
# Fetch the pinned ios-topaz baseline into a separate directory.
#
# The baseline is not vendored in this repository. Choose the fork and commit
# you actually use; the ruby work was developed on a fork of ios-topaz and the
# patches in patches/ are the delta.
#
#   BASE_REPO=https://github.com/<owner>/ios-topaz.git \
#   BASE_REF=<commit> ./scripts/fetch-base.sh upstream
#
# The script refuses to overwrite an existing destination.
set -eu

: "${BASE_REPO:?set BASE_REPO to the ios-topaz fork URL}"
: "${BASE_REF:?set BASE_REF to the pinned commit or branch}"

DEST="${1:-upstream}"

if [ -e "$DEST" ]; then
    echo "fetch-base: $DEST already exists" >&2
    exit 2
fi

git init "$DEST"
git -C "$DEST" remote add origin "$BASE_REPO"
git -C "$DEST" fetch --depth 1 origin "$BASE_REF"
git -C "$DEST" checkout --detach FETCH_HEAD

actual="$(git -C "$DEST" rev-parse HEAD)"
echo "baseline: $DEST @ $actual"
echo "next: copy patches/ into $DEST/xnu/patches/ and run the kernel workflow"
