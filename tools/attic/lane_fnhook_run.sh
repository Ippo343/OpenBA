#!/usr/bin/env bash
# Wave 9 lane FNHOOK — A/B the detour-target resolution fix.
#
# Run A: the exe as it was before this lane (detour targets resolved through
#        ResolveAddr, i.e. the port registry). Expect the four Filename detours
#        and the CompressedStream::Read guard to REFUSE to patch.
# Run B: this lane's exe (targets resolved RAW). Expect them to install.
#
# The negative control is in the same log: FindOrLoad / External::Load are NOT
# ported, so their targets are identical under both doors and both runs must
# show the same "[FNHook] detoured ExternalFactory::FindOrLoad" line. A
# difference there would mean the change did something other than what it says.
set -u
SP="$1"          # scratchpad dir holding baseline.exe and fixed.exe
OUT="$2"         # evidence dir
GAME="C:/GOG Games/Banished"
mkdir -p "$OUT"

run_one() {
    local tag="$1" exe="$2"
    echo "== [$tag] deploying $exe"
    cp "$exe" "$GAME/reimpl-x64.exe" || return 2
    rm -f "$GAME/standalone_debug.log" "$GAME/standalone_error.log" "$GAME/standalone_crash.log"
    ( cd "$GAME" && env BANISHED_AUTOSTART=1 \
                        BANISHED_FNHOOK=1 \
                        BANISHED_READ_GUARD=1 \
                        BANISHED_PAUSEMENU=900 \
                        BANISHED_PAUSEMENU_QUIT=120 \
                        ./reimpl-x64.exe ) &
    local pid=$!
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 5; waited=$((waited+5))
        if [ "$waited" -ge 420 ]; then
            echo "[$tag] TIMEOUT — killing"
            taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1
            break
        fi
    done
    wait "$pid" 2>/dev/null
    for f in debug error crash; do
        [ -f "$GAME/standalone_$f.log" ] && cp "$GAME/standalone_$f.log" "$OUT/${tag}_$f.log"
    done
    echo "[$tag] done after ${waited}s"
}

run_one baseline "$SP/baseline.exe"
run_one fixed    "$SP/fixed.exe"
echo "== evidence in $OUT"
