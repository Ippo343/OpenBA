#!/usr/bin/env bash
# tools/stress_run.sh -- run the BANISHED_STRESS town to completion and collect the numbers.
#
# WHAT IT DOES
#   1. (unless --no-build) builds the perf exe (make standalone-perf) and deploys it
#   2. clears the previous run's logs/CSV, launches reimpl-x64.exe with BANISHED_STRESS=1
#      plus the knobs below, supervises boot with tools/boot_state.sh (bails instantly on
#      a blocked modal dialog instead of burning the timeout), then waits for the process
#      to exit (the harness quits itself through the in-game pause menu after --frames)
#   3. copies standalone_stress.csv, standalone_stress_compprof.txt and the error log into
#      perf/results/<timestamp>_<label>/ and prints tools/stress_report.py's summary,
#      diffed against --baseline when given (default: perf/baselines/current.csv if present)
#
# USAGE
#   tools/stress_run.sh [--label NAME] [--speed N] [--citizens N] [--sets N] [--frames N]
#                       [--interval N] [--seed N] [--map small|medium|large]
#                       [--baseline CSV|none] [--no-build] [--debug] [--timeout SECS]
#   Any BANISHED_* variable already in the environment is passed through untouched and
#   wins over the harness defaults (StressHarness_Init only fills in UNSET variables).
#
# EXIT CODES
#   0 run completed and report printed      1 bad usage / build failed
#   2 crash (standalone_crash.log non-empty) 3 boot blocked or timed out
#   4 run did not finish within --timeout    5 no CSV produced (harness never armed)
#   6 run completed but the process crashed during teardown (numbers are valid)

set -u
cd "$(dirname "$0")/.." || exit 1
REPO="$(pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
MAKE="${MAKE:-/c/msys64/ucrt64/bin/mingw32-make.exe}"

LABEL="run"; SPEED=""; CITIZENS=""; SETS=""; FRAMES=""; INTERVAL=""; SEED=""; MAP=""
BASELINE=""; BUILD=1; DEBUG=0; TIMEOUT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --label)    LABEL="$2"; shift 2 ;;
        --speed)    SPEED="$2"; shift 2 ;;
        --citizens) CITIZENS="$2"; shift 2 ;;
        --sets)     SETS="$2"; shift 2 ;;
        --frames)   FRAMES="$2"; shift 2 ;;
        --interval) INTERVAL="$2"; shift 2 ;;
        --seed)     SEED="$2"; shift 2 ;;
        --map)      MAP="$2"; shift 2 ;;
        --baseline) BASELINE="$2"; shift 2 ;;
        --timeout)  TIMEOUT="$2"; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        --debug)    DEBUG=1; BUILD=0; shift ;;
        -h|--help)  sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 1 ;;
    esac
done

if [ "$BUILD" = 1 ]; then
    echo "== building perf exe"
    "$MAKE" standalone-perf || { echo "build failed" >&2; exit 1; }
    cp standalone/build-perf/reimpl-x64.exe "$GAME_DIR/reimpl-x64.exe" || exit 1
fi

# Frames the harness will run before quitting: default from the harness (6000) unless
# overridden. Needed here only to size the wait timeout.
FRAMES_EFF="${FRAMES:-${BANISHED_STRESS_FRAMES:-6000}}"
if [ -z "$TIMEOUT" ]; then
    # 60 sim frames per wall second when the sim keeps up; allow 3x for a slow build
    # plus 5 minutes for world build + boot.
    TIMEOUT=$(( FRAMES_EFF / 60 * 3 + 300 ))
    [ "$FRAMES_EFF" -le 0 ] && TIMEOUT=0
fi

export BANISHED_STRESS=1
[ -n "$SPEED" ]    && export BANISHED_STRESS_SPEED="$SPEED"
[ -n "$CITIZENS" ] && export BANISHED_STRESS_CITIZENS="$CITIZENS"
[ -n "$SETS" ]     && export BANISHED_STRESS_SETS="$SETS"
[ -n "$FRAMES" ]   && export BANISHED_STRESS_FRAMES="$FRAMES"
[ -n "$INTERVAL" ] && export BANISHED_STRESS_INTERVAL="$INTERVAL"
[ -n "$SEED" ]     && export BANISHED_SEED="$SEED"
[ -n "$MAP" ]      && export BANISHED_MAP_SIZE="$MAP"

if tasklist 2>/dev/null | grep -qi 'reimpl-x64.exe'; then
    echo "reimpl-x64.exe is already running -- stop it first" >&2
    exit 1
fi

rm -f "$GAME_DIR/standalone_stress.csv" "$GAME_DIR/standalone_stress_compprof.txt" \
      "$GAME_DIR/standalone_crash.log" "$GAME_DIR/standalone_error.log" \
      "$GAME_DIR/standalone_debug.log" "$GAME_DIR/standalone_boot_state.txt"

STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/perf/results/${STAMP}_${LABEL}"
mkdir -p "$OUT"

echo "== launching  label=$LABEL speed=${SPEED:-default} citizens=${CITIZENS:-default} sets=${SETS:-default} frames=$FRAMES_EFF debug=$DEBUG"
echo "   results -> $OUT"
( cd "$GAME_DIR" && ./reimpl-x64.exe ) &
LAUNCH_PID=$!

echo "== waiting for boot"
tools/boot_state.sh --wait 600
rc=$?
if [ $rc -ne 0 ]; then
    echo "boot did not reach RUNNING (boot_state rc=$rc)" >&2
    exit 3
fi

echo "== running (timeout ${TIMEOUT}s; Ctrl-C leaves the game up)"
start=$(date +%s)
last_row=""
while tasklist 2>/dev/null | grep -qi 'reimpl-x64.exe'; do
    if [ -s "$GAME_DIR/standalone_crash.log" ]; then
        echo "!! crash log appeared"; break
    fi
    if [ -f "$GAME_DIR/standalone_stress.csv" ]; then
        row="$(tail -n 1 "$GAME_DIR/standalone_stress.csv" | tr -d '\r')"
        if [ "$row" != "$last_row" ] && [ "${row:0:1}" != "#" ] && [ "${row:0:6}" != "wall_s" ]; then
            echo "   $row"; last_row="$row"
        fi
    fi
    if [ "$TIMEOUT" -gt 0 ] && [ $(( $(date +%s) - start )) -ge "$TIMEOUT" ]; then
        echo "!! timeout after ${TIMEOUT}s -- killing"
        taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1
        sleep 2
        cp -f "$GAME_DIR"/standalone_stress*.* "$GAME_DIR/standalone_error.log" "$OUT/" 2>/dev/null
        exit 4
    fi
    sleep 5
done
wait $LAUNCH_PID 2>/dev/null

cp -f "$GAME_DIR"/standalone_stress*.* "$OUT/" 2>/dev/null
cp -f "$GAME_DIR/standalone_error.log" "$OUT/" 2>/dev/null
[ -s "$GAME_DIR/standalone_crash.log" ] && cp -f "$GAME_DIR/standalone_crash.log" "$OUT/"

TEARDOWN_CRASH=0
if [ -s "$GAME_DIR/standalone_crash.log" ]; then
    # A crash AFTER the harness clicked Quit is a teardown bug, not a lost run: every CSV
    # row was written first. Report it loudly but still produce the numbers (exit 6).
    last_frame=$(grep -v '^#' "$OUT/standalone_stress.csv" 2>/dev/null | tail -n 1 | cut -d, -f3)
    if [ "$FRAMES_EFF" -gt 0 ] && [ -n "$last_frame" ] && [ "${last_frame%.*}" -ge "$FRAMES_EFF" ] 2>/dev/null        && grep -q 'PauseMenu\] dispatching' "$OUT/standalone_error.log" 2>/dev/null; then
        echo "== CRASHED DURING TEARDOWN (after the quit click at frame $last_frame) -- numbers are complete:"
        head -n 12 "$GAME_DIR/standalone_crash.log"
        TEARDOWN_CRASH=1
    else
        echo "== CRASHED -- standalone_crash.log:"
        head -n 40 "$GAME_DIR/standalone_crash.log"
        exit 2
    fi
fi
if [ ! -s "$OUT/standalone_stress.csv" ]; then
    echo "== no standalone_stress.csv was written: the harness never produced a row." >&2
    echo "   grep '\[Stress' standalone_error.log for why." >&2
    grep -m 20 '\[Stress' "$GAME_DIR/standalone_error.log" 2>/dev/null
    exit 5
fi

if [ -z "$BASELINE" ] && [ -f perf/baselines/current.csv ]; then BASELINE=perf/baselines/current.csv; fi
[ "$BASELINE" = "none" ] && BASELINE=""
echo "== report"
if [ -n "$BASELINE" ]; then
    python tools/stress_report.py "$OUT/standalone_stress.csv" --compprof "$OUT/standalone_stress_compprof.txt" --baseline "$BASELINE" | tee "$OUT/report.txt"
else
    python tools/stress_report.py "$OUT/standalone_stress.csv" --compprof "$OUT/standalone_stress_compprof.txt" | tee "$OUT/report.txt"
fi
echo "== done: $OUT"
[ "$TEARDOWN_CRASH" = 1 ] && exit 6
exit 0
