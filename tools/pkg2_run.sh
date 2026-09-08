#!/usr/bin/env bash
# tools/pkg2_run.sh — Wave 9 lane PKG2's contention-safe deploy+run+capture.
#
# WHY: six lanes deploy to the one shared game directory. This lane lost two
# measurements to a concurrent lane replacing reimpl-x64.exe between the copy
# and the launch — once silently (the logs were overwritten after the run) and
# once visibly (the run reported another lane's registration counts). Doing the
# deploy, the launch and the log copy in one process, with an identity check on
# BOTH ends, is the only shape that survives.
#
# The identity check is the registration count, not the build timestamp: the
# `[Standalone] Build:` line comes from __DATE__/__TIME__ in a ccache-reused
# object and is stale by design.
#
# USAGE: tools/pkg2_run.sh <label> <expected-IO-count> -- VAR=VAL ...
set -u
OUT=docs/runtime_port/wave9_lane_pkg2
GD="${BANISHED_DIR:-C:/GOG Games/Banished}"
LABEL="${1:?label}"; shift
WANT="${1:?expected IO export count}"; shift
[ "${1:-}" = "--" ] && shift

MINE=$(md5sum standalone/build/reimpl-x64.exe | cut -d' ' -f1)
mkdir -p "$OUT"

for attempt in 1 2 3 4 5; do
    # Wait for whoever holds the exe to finish.
    while tasklist 2>/dev/null | grep -qi reimpl-x64; do sleep 6; done

    if ! cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" 2>/dev/null; then
        echo "attempt $attempt: exe busy at copy time, retrying"
        sleep 5; continue
    fi

    NOW=$(md5sum "$GD/reimpl-x64.exe" | cut -d' ' -f1)
    if [ "$NOW" != "$MINE" ]; then
        echo "attempt $attempt: CLOBBERED between copy and launch, retrying"
        sleep 5; continue
    fi

    # The package set is part of this lane's input, so it is recorded next to
    # the binary identity — a .pkm appearing in WinData changes what
    # LoadPackages mounts even when the binary is right.
    echo "attempt $attempt: pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

    tools/rt_run.sh "$LABEL" --timeout 600 -- "$@" > "$OUT/rt_run_${LABEL}_stdout.txt" 2>&1
    cp "$GD/standalone_error.log" "$OUT/${LABEL}_error.log" 2>/dev/null
    cp "$GD/standalone_debug.log" "$OUT/${LABEL}_debug.log" 2>/dev/null
    cp "$GD/standalone_crash.log" "$OUT/${LABEL}_crash.log" 2>/dev/null

    GOT=$(grep -m1 -oE "IO: [0-9]+ exports" "$OUT/${LABEL}_debug.log" 2>/dev/null | grep -oE '[0-9]+')
    echo "attempt $attempt: IO=$GOT (want $WANT)"
    if [ "$GOT" = "$WANT" ]; then
        echo "IDENTITY OK on attempt $attempt"
        exit 0
    fi
    echo "attempt $attempt: ran ANOTHER LANE'S EXE — discarding and retrying"
    sleep 5
done
echo "pkg2_run: gave up after 5 attempts; contention too high"
exit 1
