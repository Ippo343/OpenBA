#!/usr/bin/env bash
# Fetch dxvk-native 3.1.1 (the Steam Runtime "sniper" build) for the Linux game build.
# D4_RULES section 3 Q6: the pinned release, fetched by script and checked against its sha256;
# the binaries are never committed (third_party/dxvk-native/ is gitignored).
# Pin: docs/runtime_port/phase_d/plans/RENDER.md section B (the release page's sha256).
#
# usage: tools/fetch_dxvk_native.sh            -> third_party/dxvk-native/3.1.1/{usr/...}
# Idempotent: a tree that already holds the stamp is left alone. Runs on Linux (WSL) or MSYS.
set -euo pipefail
VER=3.1.1
SHA256=506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7
NAME=dxvk-native-${VER}-steamrt-sniper.tar.gz
URL=https://github.com/doitsujin/dxvk/releases/download/v${VER}/${NAME}

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST="$ROOT/third_party/dxvk-native/$VER"
STAMP="$DEST/.sha256"
if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$SHA256" ]; then
  echo "dxvk-native $VER already at $DEST"
  exit 0
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
echo "fetching $URL"
curl -fL --retry 3 -o "$TMP/$NAME" "$URL"
GOT=$(sha256sum "$TMP/$NAME" | cut -d' ' -f1)
if [ "$GOT" != "$SHA256" ]; then
  echo "sha256 MISMATCH: got $GOT, want $SHA256" >&2
  exit 1
fi
rm -rf "$DEST"
mkdir -p "$DEST"
tar -xzf "$TMP/$NAME" -C "$DEST"
echo "$SHA256" > "$STAMP"
echo "dxvk-native $VER extracted to $DEST (sha256 OK)"
