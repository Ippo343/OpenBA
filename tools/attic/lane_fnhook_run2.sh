#!/usr/bin/env bash
# Wave 9 lane FNHOOK — run 2: the effectiveness control + the default-flags
# regression control, both on the FIXED exe.
#
#   instr   BANISHED_FNHOOK=1 BANISHED_READ_GUARD=1 — the five newly-live
#           detours are exercised through a full world build. The exit census
#           says how many times each wrapper actually RAN, which "detoured X"
#           does not.
#   default no diagnostic flags at all — proves the change is inert on the path
#           every other lane measures. All five newly-live targets sit behind
#           opt-in env gates, so this must be indistinguishable from before.
set -u
EXE="$1"; OUT="$2"
GAME="C:/GOG Games/Banished"
mkdir -p "$OUT"
cp "$EXE" "$GAME/reimpl-x64.exe" || exit 2

run_one() {
    local tag="$1"; shift
    rm -f "$GAME/standalone_debug.log" "$GAME/standalone_error.log" "$GAME/standalone_crash.log"
    ( cd "$GAME" && env "$@" ./reimpl-x64.exe ) &
    local pid=$! waited=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 5; waited=$((waited+5))
        if [ "$waited" -ge 420 ]; then
            echo "[$tag] TIMEOUT — killing"; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; break
        fi
    done
    wait "$pid" 2>/dev/null
    for f in debug error crash; do
        [ -f "$GAME/standalone_$f.log" ] && cp "$GAME/standalone_$f.log" "$OUT/${tag}_$f.log"
    done
    echo "[$tag] exited after ${waited}s"
}

run_one instr   BANISHED_AUTOSTART=1 BANISHED_FNHOOK=1 BANISHED_READ_GUARD=1 \
                BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
run_one default BANISHED_AUTOSTART=1 \
                BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
echo "== evidence in $OUT"
