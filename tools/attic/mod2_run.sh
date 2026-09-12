#!/usr/bin/env bash
# =============================================================================
# tools/mod2_run.sh -- Wave 9 lane MOD (second pass): deploy, mount, run, unmount
# =============================================================================
#
# THE PACKAGE SET IS THE MEASUREMENT, NOT A DETAIL.
#
# ---------------------------------------------------------------------------
# `RefreshList`'s entire body is a loop over `WinData/*.pkm`. With the mods in
# `WinData/_mods_off/` -- which is how the shared game directory is normally
# left, because the achievement lanes need them out -- `FindFiles` returns zero
# entries, LOOP 1 never executes, the mod list stays empty and the oracle
# reports `divergences=0` over two empty photographs. That is a green line
# meaning "never ran" (reference_uniform_population_is_not_a_verification.md),
# and it is the single most likely way this lane could ship on a lie.
#
# So this script MOUNTS the two .pkm for the duration of its runs and moves them
# back afterwards, under the same lock, and it PRINTS the set it ran against.
# The lock does not cover WinData -- the integrator is told separately.
#
# TWO MODS IS THE MINIMUM, not a nicety. The override pass (LOOP 2) is a double
# loop over ordered pairs i != j: at n=1 its body never executes and every term
# it could get wrong is unstateable. The `files` array, which is the pass's only
# output, is empty for every mod at n=1 no matter what the code does.
#
# USAGE: tools/mod2_run.sh [--no-acquire]
# =============================================================================
set -u

OUT=docs/runtime_port/wave9_lane_mod2
GD="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LANE=lane-MOD
ACQUIRE=1
[ "${1:-}" = "--no-acquire" ] && ACQUIRE=0

# The identity line, derived from THIS build's own source rather than hardcoded
# -- a hardcoded count is how a gate goes stale and starts passing an exe that
# never contained the change (tools/pkg3_run.sh's header records that failure).
WANT=$(grep -cE '^ *RegisterRuntimeExport\(' runtime/src/io/modification_refresh.cpp)
echo "[mod2] identity gate: expecting '(lane MOD2): ${WANT} exports + 8 addresses'"

mkdir -p "$OUT"

if [ "$ACQUIRE" = "1" ]; then
    bash tools/game_dir_lock.sh acquire "$LANE" 900 || {
        echo "[mod2] could not acquire the game-directory lock"; exit 1; }
fi

restore_mods() {
    for f in betterFields.pkm DebugMenu.pkm; do
        [ -f "$GD/WinData/$f" ] && mv -f "$GD/WinData/$f" "$GD/WinData/_mods_off/$f"
    done
    echo "[mod2] .pkm returned to _mods_off: $(ls "$GD/WinData/"*.pkm 2>/dev/null | wc -l) left mounted"
    bash tools/game_dir_lock.sh release "$LANE"
}
trap restore_mods EXIT

cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || {
    echo "[mod2] deploy failed (exe busy?) -- another lane may not be taking the lock"
    exit 1; }

for f in betterFields.pkm DebugMenu.pkm; do
    [ -f "$GD/WinData/_mods_off/$f" ] && cp "$GD/WinData/_mods_off/$f" "$GD/WinData/$f"
done
echo "[mod2] MOUNTED pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

run_one() {
    local label="$1"; shift
    tools/rt_run.sh "$label" --timeout 600 -- "$@" > "$OUT/rt_run_${label}_stdout.txt" 2>&1
    local rc=$?
    cp "$GD/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
    cp "$GD/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
    cp "$GD/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
    # THE IDENTITY GATE, read from THIS RUN'S OWN LOG. Not a timestamp, not an
    # md5: the shared directory means another lane can overwrite the exe between
    # the cp and the launch, and a clobbered run produces a plausible, clean,
    # WRONG measurement (tools/game_dir_lock.sh's header records three of them).
    if grep -aq "(lane MOD2): ${WANT} exports + 8 addresses" "$OUT/${label}_error.log" \
                                                             "$OUT/${label}_debug.log" 2>/dev/null; then
        echo "[mod2] $label: IDENTITY OK (rc=$rc)"
    else
        echo "[mod2] $label: *** IDENTITY MISSING -- this run did not execute our exe (rc=$rc)"
    fi
    grep -a "IO::Modification::RefreshList" "$OUT/${label}_error.log" "$OUT/${label}_debug.log" 2>/dev/null | head -4
}

# Run A -- the mods menu, driven unattended. RefreshList runs once from
# Engine::Startup and again from the dialog's Cancel/Ok path, so this is the run
# with more than one call.
run_one menu BANISHED_RT_DIFF=1 BANISHED_AUTOMENU_BUTTON=7 \
        BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1

# Runs B..D -- the negative controls. RefreshList runs from Engine::Startup on
# any boot, so a menu-and-quit run is enough to fire them. Each MUST report a
# divergence naming the term it broke; a control that cannot fire is worse than
# none (reference_probe_needs_an_effectiveness_control.md).
for n in 1 2 3; do
    run_one "neg$n" BANISHED_RT_DIFF=1 BANISHED_RT_MOD_NEGCTL=$n \
            BANISHED_AUTOMENU_BUTTON=6
done

echo "[mod2] done; evidence in $OUT"
