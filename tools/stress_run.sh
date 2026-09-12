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
# WHAT ELSE THE RESULTS DIRECTORY NOW CONTAINS, AND WHY (Wave 26)
#
#   machine_load.csv / machine_load.txt
#     For much of Wave 25, up to SEVEN lanes were compiling and running Python on
#     this machine at once. A perf number taken under unknown contention is not
#     comparable to one taken under different unknown contention -- and nothing
#     recorded the contention, so the comparison could neither be made nor
#     refused. It was simply unknown, and unknown reads like fine.
#
#     So the poll loop now samples CPU load, the process table, and free memory
#     for the life of the run, and writes a per-run summary. THIS DOES NOT MAKE
#     TWO RUNS COMPARABLE. It makes them ARGUABLE: a later reader can see that
#     run A had four cc1plus processes alive throughout and run B had none, and
#     decide what to do about it. That is the whole claim.
#
#   exe_identity.txt
#     Wave 25 lane ALLOC md5-verified the deployed exe against its own build
#     before every launch. That per-run check is the only reason "was my
#     measurement contaminated?" was answerable rather than arguable, and it was
#     per-lane discipline. Recorded here automatically, before and after the run.
#
# USAGE
#   tools/stress_run.sh [--label NAME] [--speed N] [--citizens N] [--sets N] [--frames N]
#                       [--interval N] [--seed N] [--map small|medium|large]
#                       [--baseline CSV|none] [--no-build] [--debug] [--timeout SECS]
#                       [--lock LANE] [--no-load]
#   --lock LANE   deploy and run under tools/game_dir_lock.sh, held across the whole
#                 run (OPT-IN, not the default -- see WAVE26_LANE_LOCK.md section 6)
#   --no-load     skip machine-load sampling
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

ORIG_ARGS=("$@")
LABEL="run"; SPEED=""; CITIZENS=""; SETS=""; FRAMES=""; INTERVAL=""; SEED=""; MAP=""
BASELINE=""; BUILD=1; DEBUG=0; TIMEOUT=""; LOCK_LANE=""; LOAD=1
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
        --lock)     LOCK_LANE="$2"; shift 2 ;;
        --no-load)  LOAD=0; shift ;;
        --no-build) BUILD=0; shift ;;
        --debug)    DEBUG=1; BUILD=0; shift ;;
        -h|--help)  sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 1 ;;
    esac
done

# ---- MACHINE LOAD ------------------------------------------------------------
#
# Deliberately CHEAP, because the sampler must not be the contention. Measured
# on this machine: `tasklist` 0.26s, `wmic OS get FreePhysicalMemory` 0.09s,
# `wmic cpu get loadpercentage` 1.09s. The poll loop runs every 5s, so the
# tasklist/memory pair is taken every poll and the expensive CPU reading every
# LOAD_CPU_EVERY-th poll (CPU column is empty on the others rather than
# carried forward -- a repeated value would read as a measurement).
LOAD_CPU_EVERY=6

cpu_load() { wmic cpu get loadpercentage 2>/dev/null | tr -d '\r' | sed -n '2p' | tr -d ' '; }
free_mb()  { local k; k=$(wmic OS get FreePhysicalMemory 2>/dev/null | tr -d '\r' | sed -n '2p' | tr -d ' '); case "$k" in ''|*[!0-9]*) printf '' ;; *) printf '%s' $(( k / 1024 )) ;; esac; }

# One tasklist call, every count derived from it. Counting separately would be
# several calls AND several different instants.
LOAD_HDR='t_s,cpu_pct,free_mb,procs,make,compiler,python,reimpl,other_game'
machine_sample() {   # $1 = seconds since start, $2 = 1 to take the CPU reading
    local t="$1" want_cpu="$2" tl cpu procs mk cc py re og
    tl=$(tasklist 2>/dev/null)
    procs=$(printf '%s\n' "$tl" | grep -c -iE '\.exe')
    mk=$(printf '%s\n'   "$tl" | grep -c -iE '^(mingw32-)?make\.exe')
    cc=$(printf '%s\n'   "$tl" | grep -c -iE '^(cc1plus|cc1|g\+\+|gcc|ld|as|collect2)\.exe')
    py=$(printf '%s\n'   "$tl" | grep -c -iE '^python')
    re=$(printf '%s\n'   "$tl" | grep -c -i  'reimpl-x64\.exe')
    og=$(printf '%s\n'   "$tl" | grep -c -iE '^(Application-x64|Tools-x64)')
    cpu=""
    [ "$want_cpu" = 1 ] && cpu=$(cpu_load)
    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$t" "$cpu" "$(free_mb)" "$procs" "$mk" "$cc" "$py" "$re" "$og"
}

# ---- OPT-IN LOCK -------------------------------------------------------------
# This script DEPLOYS and LAUNCHES -- exactly what tools/game_dir_lock.sh
# protects -- and until Wave 26 it neither acquired nor asserted. Making it
# mandatory now would turn a harness six lanes depend on into one that can
# refuse to run, so it is opt-in and the default is unchanged. ADDRESSEE: the
# Wave 26/27 integrator decides whether this becomes the default.
#
# Re-exec rather than acquire-here, so the acquire's exit status is checked
# inside game_dir_lock.sh's own `run` where no caller's pipe can discard it.
if [ -n "$LOCK_LANE" ] && [ "${STRESS_RUN_UNDER_LOCK:-0}" != "1" ]; then
    export STRESS_RUN_UNDER_LOCK=1
    exec "$REPO/tools/game_dir_lock.sh" run "$LOCK_LANE" 1800 -- \
        "$REPO/tools/stress_run.sh" ${ORIG_ARGS[@]+"${ORIG_ARGS[@]}"}
fi

# Exercise the sampler itself, in THIS file, without running anything. A copy of
# these functions in a test script would be a second implementation agreeing
# with itself -- the shape where two checks share one hidden premise and read as
# independent. This runs the real code.
if [ "${STRESS_LOAD_PROBE:-0}" = "1" ]; then
    echo "$LOAD_HDR"
    machine_sample -1 1
    machine_sample 0 0
    exit 0
fi

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

# ---- EXE IDENTITY, BEFORE THE LAUNCH ----------------------------------------
# ALLOC's discipline, recorded rather than remembered. `built` is empty when
# --no-build/--debug was used, which is itself the fact worth recording: the run
# is against whatever was already deployed, and nothing here can say whose.
BUILT_EXE="standalone/build-perf/reimpl-x64.exe"
[ "$DEBUG" = 1 ] && BUILT_EXE="standalone/build/reimpl-x64.exe"
{
    echo "deployed_before=$(md5sum "$GAME_DIR/reimpl-x64.exe" 2>/dev/null | cut -c1-32)"
    echo "built=$(md5sum "$BUILT_EXE" 2>/dev/null | cut -c1-32)   ($BUILT_EXE)"
    echo "built_this_run=$BUILD"
    echo "lock=${LOCK_LANE:-<none held>}"
} > "$OUT/exe_identity.txt"
if [ "$BUILD" = 1 ]; then
    a=$(md5sum "$GAME_DIR/reimpl-x64.exe" 2>/dev/null | cut -c1-32)
    b=$(md5sum "$BUILT_EXE" 2>/dev/null | cut -c1-32)
    if [ -n "$a" ] && [ "$a" != "$b" ]; then
        echo "!! the deployed exe is NOT the one just built -- somebody deployed over it." >&2
        echo "   deployed=$a built=$b" >&2
        exit 1
    fi
fi

# The header file is written even with --no-load. A results directory with NO
# machine_load.csv is silently absent, and a silently absent load record reads
# exactly like a quiet machine -- which is the failure lane ATTEND reported in
# another form on the same day (a stale cutover_manifest.inc survived a whole
# wave because its count MATCHED the prediction). An always-present file whose
# reporter says UNEXAMINED makes "nothing was measured" a statement rather than
# a gap the reader has to notice.
echo "$LOAD_HDR" > "$OUT/machine_load.csv"
if [ "$LOAD" = 1 ]; then
    machine_sample -1 1 >> "$OUT/machine_load.csv"      # before the launch
else
    echo "# --no-load was passed: NOTHING WAS SAMPLED. This is not a quiet machine." >> "$OUT/machine_load.csv"
fi

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
poll=0
while tasklist 2>/dev/null | grep -qi 'reimpl-x64.exe'; do
    if [ "$LOAD" = 1 ]; then
        want_cpu=0
        [ $(( poll % LOAD_CPU_EVERY )) -eq 0 ] && want_cpu=1
        machine_sample "$(( $(date +%s) - start ))" "$want_cpu" >> "$OUT/machine_load.csv"
    fi
    poll=$(( poll + 1 ))
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

if [ "$LOAD" = 1 ]; then
    machine_sample "$(( $(date +%s) - start ))" 1 >> "$OUT/machine_load.csv"
fi
# The identity question, closed at the other end: if the deployed exe is not the
# one that was there when this run started, something landed on it mid-run and
# the numbers are about an unknown mixture. This is ALLOC's per-launch md5,
# taken twice so it can answer for the INTERVAL and not just the instant.
{
    echo "deployed_after=$(md5sum "$GAME_DIR/reimpl-x64.exe" 2>/dev/null | cut -c1-32)"
} >> "$OUT/exe_identity.txt"
IDENT_BEFORE=$(sed -n 's/^deployed_before=//p' "$OUT/exe_identity.txt" | head -n 1)
IDENT_AFTER=$(sed -n 's/^deployed_after=//p' "$OUT/exe_identity.txt" | head -n 1)
if [ -n "$IDENT_BEFORE" ] && [ "$IDENT_BEFORE" != "$IDENT_AFTER" ]; then
    echo "!! EXE CHANGED DURING THIS RUN: $IDENT_BEFORE -> $IDENT_AFTER" | tee -a "$OUT/exe_identity.txt"
    echo "!! These numbers are NOT from one binary. Discard them." | tee -a "$OUT/exe_identity.txt"
fi

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
echo "== machine load"
# Appended to report.txt, not printed beside it: a contention line in a separate
# file is a line nobody reads next to the number it qualifies. Emitted
# UNCONDITIONALLY -- with --no-load the file holds only a header and the
# reporter says UNEXAMINED, which is the point: the reader is told that nothing
# was measured instead of being told nothing.
[ "$LOAD" = 1 ] || echo "(--no-load was passed)" | tee -a "$OUT/report.txt"
python tools/machine_load_report.py "$OUT/machine_load.csv" | tee -a "$OUT/report.txt"
echo "== exe identity"
cat "$OUT/exe_identity.txt"
echo "== done: $OUT"
[ "$TEARDOWN_CRASH" = 1 ] && exit 6
exit 0
