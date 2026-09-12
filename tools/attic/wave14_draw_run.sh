#!/usr/bin/env bash
# Wave 14 lane DRAW — one save-test run of the deployed exe, logs archived.
# Usage: tools/wave14_draw_run.sh <label> [EXTRA_ENV_ASSIGNMENTS...]
# The caller must already hold tools/game_dir_lock.sh.
set -u
LABEL="$1"; shift
GD="C:/GOG Games/Banished"
OUT="docs/runtime_port/wave14/draw_${LABEL}"
mkdir -p "$OUT"

cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || exit 1
rm -f "$GD/standalone_debug.log" "$GD/standalone_error.log" "$GD/standalone_crash.log"

# Save at frame 400 (completes ~frame 461); open the pause menu at 600 and click
# Quit 60 frames later so the process exits normally and RtDiffSummary() prints.
cd "$GD" || exit 1
timeout 900 env BANISHED_AUTOSTART=1 \
    BANISHED_SAVE_TEST=400 \
    BANISHED_PAUSEMENU=600 \
    BANISHED_PAUSEMENU_QUIT=60 \
    BANISHED_PAUSEMENU_BUTTON=6 \
    "$@" \
    ./reimpl-x64.exe
RC=$?
cd - >/dev/null || exit 1

cp "$GD/standalone_error.log" "$OUT/error.log" 2>/dev/null
cp "$GD/standalone_debug.log" "$OUT/debug.log" 2>/dev/null
cp "$GD/standalone_crash.log" "$OUT/crash.log" 2>/dev/null
echo "rc=$RC label=$LABEL" | tee "$OUT/rc.txt"
