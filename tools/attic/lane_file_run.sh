#!/usr/bin/env bash
# tools/lane_file_run.sh — Wave 19 lane FILE's measured run.
#
# WHY THIS EXISTS RATHER THAN A HAND-TYPED LAUNCH: lane FILE's first attempt
# launched the exe directly from bash, which RETURNED IMMEDIATELY (the process
# detaches), so `game_dir_lock.sh run` released the lock while the run was still
# in flight. Another lane deployed over the exe inside that window and the
# measurement was taken against a binary that was 16,599 bytes different from
# the one this lane built. It was caught by the missing `[RtPort] ... lane FILE`
# registration line, and by comparing the deployed exe's SIZE against the
# build's — not by anything in the run itself.
#
# tools/rt_run.sh already solves exactly this (it refuses to launch over a live
# process, waits for exit, and fails fast on a blocked boot). This script is the
# deploy + verify wrapper around it.
set -u
WT="$(cd "$(dirname "$0")/.." && pwd)"
GD="${BANISHED_DIR:-C:/GOG Games/Banished}"
LABEL="${1:?usage: lane_file_run.sh <label> -- ENV=VAL ...}"; shift
[ "${1:-}" = "--" ] && shift

cd "$WT" || exit 2
"$WT/tools/game_dir_lock.sh" assert-held lane-FILE || exit 2

cp "$WT/standalone/build/reimpl-x64.exe" "$GD/reimpl-x64.exe" || exit 3

# PROVE the deployed bytes are ours before measuring anything.
A=$(md5sum < "$WT/standalone/build/reimpl-x64.exe" | cut -d' ' -f1)
B=$(md5sum < "$GD/reimpl-x64.exe" | cut -d' ' -f1)
if [ "$A" != "$B" ]; then
    echo "lane_file_run[$LABEL]: DEPLOYED EXE IS NOT OURS ($A vs $B) -- refusing"
    exit 4
fi
echo "lane_file_run[$LABEL]: deployed exe md5 $A CONFIRMED ours"

"$WT/tools/rt_run.sh" "$LABEL" --timeout 900 -- "$@"
RC=$?

C=$(md5sum < "$GD/reimpl-x64.exe" | cut -d' ' -f1)
if [ "$C" != "$A" ]; then
    echo "lane_file_run[$LABEL]: THE EXE CHANGED UNDER THE RUN ($A -> $C)."
    echo "lane_file_run[$LABEL]: this measurement is VOID."
    exit 5
fi
echo "lane_file_run[$LABEL]: exe md5 unchanged across the run -- measurement stands"
exit $RC
