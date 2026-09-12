#!/usr/bin/env bash
# =============================================================================
# tools/mod2_run3.sh -- lane MOD (2nd pass): the rewritten set controls, the
#                       CONTROL run, and the audit denominators, before and after
# =============================================================================
#
# tools/mod2_run2.sh tried to make the control build with `git stash push -- <two
# paths>`. That SILENTLY DID NOTHING: modification_refresh.cpp is a NEW file, and
# `git stash push` with explicit paths ignores untracked ones unless told
# otherwise. The script noticed only because it checked its own guard file, and
# the control was skipped rather than measured against the wrong tree -- but the
# lesson is the usual one, that an instrument which cannot fail loudly will fail
# quietly. This version moves the file aside by hand and checks out the BASE
# COMMIT's modification.cpp, then verifies the identity line is ABSENT from the
# control run's log, which is the only proof the control really was a control.
#
# It also takes the audit denominators in BOTH states, in one pass, so the
# set-diff is over the same tree and the same tool versions.
#
# USAGE: tools/mod2_run3.sh
# =============================================================================
set -u

OUT=docs/runtime_port/wave9_lane_mod2
GD="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LANE=lane-MOD
MAKE=/c/msys64/ucrt64/bin/mingw32-make.exe
BASE=fd30180
STASHDIR=.mod2_aside

WANT=$(grep -cE '^ *RegisterRuntimeExport\(' runtime/src/io/modification_refresh.cpp)
mkdir -p "$OUT"

# ---- AFTER: the audit denominators with this lane present -------------------
python tools/addr_door_audit.py        > "$OUT/audit_after_addrdoor.txt" 2>&1
python tools/unsatisfied_export_scan.py > "$OUT/audit_after_unsat.txt"   2>&1
python tools/port_bypass_audit.py      > "$OUT/audit_after_bypass.txt"   2>&1
python tools/rawdoor_audit.py          > "$OUT/audit_after_rawdoor.txt"  2>&1
python tools/detour_target_audit.py    > "$OUT/audit_after_detour.txt"   2>&1
python tools/runtime_census.py         > "$OUT/audit_after_census.txt"   2>&1
echo "[mod2] AFTER audits captured"

bash tools/game_dir_lock.sh acquire "$LANE" 1800 || {
    echo "[mod2] could not acquire the game-directory lock"; exit 1; }

restore() {
    if [ -d "$STASHDIR" ]; then
        mv -f "$STASHDIR/modification_refresh.cpp" runtime/src/io/ 2>/dev/null
        rmdir "$STASHDIR" 2>/dev/null
        git checkout HEAD -- runtime/src/io/modification.cpp
        echo "[mod2] lane source restored"
    fi
    for f in betterFields.pkm DebugMenu.pkm; do
        [ -f "$GD/WinData/$f" ] && mv -f "$GD/WinData/$f" "$GD/WinData/_mods_off/$f"
    done
    echo "[mod2] .pkm mounted after cleanup: $(ls "$GD/WinData/"*.pkm 2>/dev/null | wc -l)"
    bash tools/game_dir_lock.sh release "$LANE"
}
trap restore EXIT

for f in betterFields.pkm DebugMenu.pkm; do
    [ -f "$GD/WinData/_mods_off/$f" ] && cp "$GD/WinData/_mods_off/$f" "$GD/WinData/$f"
done
echo "[mod2] MOUNTED pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

run_one() {  # run_one <label> <expect-identity 0|1> ENV...
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
        echo "[mod2] $label: *** IDENTITY WRONG (present=$seen want=$wantId rc=$rc)"
    fi
    grep -a "RefreshList" "$OUT/${label}_error.log" 2>/dev/null | grep -a "calls=\|DIVERGENCE"
}

# ---- the two set controls, on the lane build --------------------------------
cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || exit 1
for n in 2 4; do
    run_one "neg$n" 1 BANISHED_RT_DIFF=1 BANISHED_RT_MOD_NEGCTL=$n BANISHED_AUTOMENU_BUTTON=6
done

# ---- the control: same harness as run A, on a build WITHOUT this lane -------
mkdir -p "$STASHDIR"
mv runtime/src/io/modification_refresh.cpp "$STASHDIR/" || exit 1
git checkout "$BASE" -- runtime/src/io/modification.cpp || exit 1
"$MAKE" standalone-debug >"$OUT/control_build.txt" 2>&1 || {
    echo "[mod2] control build FAILED -- see $OUT/control_build.txt"; exit 1; }

# ---- BEFORE: the same audits on the same tree, lane absent ------------------
python tools/addr_door_audit.py        > "$OUT/audit_before_addrdoor.txt" 2>&1
python tools/unsatisfied_export_scan.py > "$OUT/audit_before_unsat.txt"   2>&1
python tools/port_bypass_audit.py      > "$OUT/audit_before_bypass.txt"   2>&1
python tools/rawdoor_audit.py          > "$OUT/audit_before_rawdoor.txt"  2>&1
python tools/detour_target_audit.py    > "$OUT/audit_before_detour.txt"   2>&1
python tools/runtime_census.py         > "$OUT/audit_before_census.txt"   2>&1
echo "[mod2] BEFORE audits captured"

cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || exit 1
run_one control 0 BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=7 \
        BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1

# restore() puts the lane's files back; rebuild so the deployed tree matches.
mv -f "$STASHDIR/modification_refresh.cpp" runtime/src/io/
rmdir "$STASHDIR"
git checkout HEAD -- runtime/src/io/modification.cpp
"$MAKE" standalone-debug >"$OUT/relane_build.txt" 2>&1 || echo "[mod2] rebuild FAILED"

echo "[mod2] done; evidence in $OUT"
