#!/usr/bin/env bash
# Wave 21 lane VFY item 4 -- full, untruncated RtMintShadow capture, snapshot
# taken BEFORE lock release so a concurrent lane cannot clobber it first.
set -u
G="C:/GOG Games/Banished"
SRC="$(cd "$(dirname "$0")/.." && pwd)"

cp "$SRC/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"

cd "$G" || exit 1
BANISHED_RT_MINT_SHADOW=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 \
  BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe
echo "EXE EXIT=$?"
