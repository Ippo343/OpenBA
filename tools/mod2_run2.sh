#!/usr/bin/env bash
# =============================================================================
# tools/mod2_run2.sh -- lane MOD (2nd pass): the BY-ADDRESS door, and the CONTROL
# =============================================================================
#
# Two things tools/mod2_run.sh could not do.
#
# 1. THE BY-ADDRESS DOOR. Run A reached RefreshList once, from Engine::Startup,
#    which resolves it by NAME. The other door is
#    engine/funcs/engine_funcs.cpp's `fn_Modification_RefreshList`, resolved from
#    the raw address 0x1800af5f0, and code/Game/Dialog/moddialog.cpp:747 calls it
#    on the mods dialog's CANCEL (element id 100). Run A clicked 101 (Ok), whose
#    Apply path with BANISHED_MODAL_ANSWER=1 takes the SaveList branch and never
#    calls RefreshList -- so `calls=1` was the by-name door only. Clicking 100
#    instead exercises the address door, and a port registered on only one of the
#    two would show up here as a call count that did not move.
#
# 2. A CONTROL. Run A's log carried two [RtDiff] lines this lane does not own
#    (IO::PackageFile::ReadPackfile at +0x90, and Object::Factory::Startup's
#    probe). Whether those are pre-existing or something this lane caused is not
#    answerable from one run: it needs the SAME harness on the SAME game
#    directory with the SAME package set against a build WITHOUT this lane's two
#    files. That is what `control` is. Without it the honest statement would have
#    to be "two divergences, cause unknown".
#
# USAGE: tools/mod2_run2.sh
# =============================================================================
set -u

OUT=docs/runtime_port/wave9_lane_mod2
GD="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LANE=lane-MOD
MAKE=/c/msys64/ucrt64/bin/mingw32-make.exe

WANT=$(grep -cE '^ *RegisterRuntimeExport\(' runtime/src/io/modification_refresh.cpp)
mkdir -p "$OUT"

bash tools/game_dir_lock.sh acquire "$LANE" 900 || {
    echo "[mod2] could not acquire the game-directory lock"; exit 1; }

cleanup() {
    for f in betterFields.pkm DebugMenu.pkm; do
        [ -f "$GD/WinData/$f" ] && mv -f "$GD/WinData/$f" "$GD/WinData/_mods_off/$f"
    done
    # Put the lane's own source back no matter how this exits: a stash left on
    # the floor is a worse failure than a lost run.
    if [ -f .mod2_stashed ]; then
        git stash pop >/dev/null 2>&1 && rm -f .mod2_stashed
        echo "[mod2] source restored from stash"
    fi
    echo "[mod2] .pkm mounted after cleanup: $(ls "$GD/WinData/"*.pkm 2>/dev/null | wc -l)"
    bash tools/game_dir_lock.sh release "$LANE"
}
trap cleanup EXIT

for f in betterFields.pkm DebugMenu.pkm; do
    [ -f "$GD/WinData/_mods_off/$f" ] && cp "$GD/WinData/_mods_off/$f" "$GD/WinData/$f"
done
echo "[mod2] MOUNTED pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

run_one() {  # run_one <label> <expect-identity:0|1> ENV...
    local label="$1"; local wantId="$2"; shift 2
    tools/rt_run.sh "$label" --timeout 600 -- "$@" > "$OUT/rt_run_${label}_stdout.txt" 2>&1
    local rc=$?
    cp "$GD/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
    cp "$GD/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
    cp "$GD/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
    local seen=0
    grep -aq "(lane MOD2): ${WANT} exports + 8 addresses" \
        "$OUT/${label}_error.log" "$OUT/${label}_debug.log" 2>/dev/null && seen=1
    if [ "$seen" = "$wantId" ]; then
        echo "[mod2] $label: IDENTITY AS EXPECTED (present=$seen, rc=$rc)"
    else
        echo "[mod2] $label: *** IDENTITY WRONG (present=$seen want=$wantId rc=$rc) -- wrong exe"
    fi
}

# ---- 1. the by-address door: Cancel (id 100) ------------------------------
cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || exit 1
run_one cancel 1 BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=7 \
        BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,100,m6 BANISHED_MODAL_ANSWER=1

# ---- 2. the control: the SAME harness as run A, on a build without this lane -
git stash push -- runtime/src/io/modification_refresh.cpp runtime/src/io/modification.cpp \
    >/dev/null 2>&1 && touch .mod2_stashed
if [ ! -f .mod2_stashed ]; then echo "[mod2] stash failed; skipping control"; exit 1; fi
"$MAKE" standalone-debug >"$OUT/control_build.txt" 2>&1 || {
    echo "[mod2] control build FAILED -- see $OUT/control_build.txt"; exit 1; }
cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || exit 1
run_one control 0 BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=7 \
        BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1

git stash pop >/dev/null 2>&1 && rm -f .mod2_stashed
"$MAKE" standalone-debug >"$OUT/relane_build.txt" 2>&1 || echo "[mod2] rebuild FAILED"

echo "[mod2] done; evidence in $OUT"
