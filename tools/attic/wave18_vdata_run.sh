#!/usr/bin/env bash
# =============================================================================
# tools/wave18_vdata_run.sh -- Wave 18 lane VDATA's run batch, in ONE lock hold
# =============================================================================
#
# Two runs, identical in every respect except the one variable under test:
#
#   CONTROL  nothing armed
#   ARMED    BANISHED_RT_MINT_ROWS=<the 11 candidate rows>
#
# Everything else is shared so the diff between the two logs is the promotion
# and nothing else. Both halves carry BANISHED_RT_DIFF=1, because "the boot
# still works" is also what a promotion that did NOTHING would report
# (reference_probe_needs_an_effectiveness_control.md).
#
# BANISHED_FORCE_DEVICE_LOST=400 is the load-bearing one. Lane CEIL and the
# Wave 18 kickoff both record the ten OnVideoError typeinfos as having NO
# harness route and prescribe synthesising a call. This driver
# (dx11/src/contextdata.cpp, Wave 9 lane ENGIO) injects one synthetic
# DXGI_ERROR_DEVICE_RESET into a Present result and lets the real teardown run,
# and WAVE9_LANE_VIDL.md §4.2 records it reaching the ported body with calls=1.
#
# Usage:  tools/wave18_vdata_run.sh <outdir>
# The caller must ALREADY hold the lock; this script asserts it and refuses
# otherwise. It never acquires, so it can never be the thing that deadlocks.
set -u

OUT="${1:?usage: wave18_vdata_run.sh <outdir>}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"

"$HERE/tools/game_dir_lock.sh" assert-held vdata || {
    echo "wave18_vdata_run: lock NOT held by vdata -- refusing to deploy."; exit 2; }

mkdir -p "$OUT"

# The eleven candidate rows. Ten are the OnVideoError typeinfos (six Video, four
# Graphics); the eleventh is the vtable this lane closed the last seven traps of.
ROWS='??_7ImageBuffer@Video@@6B@'
ROWS="$ROWS,?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@PixelProgram@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@VertexProgram@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@VideoProgram@Video@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@FrameCopyImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A"
ROWS="$ROWS,?_typeInfo@ReflectionImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A"

COMMON="BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_W17_ATTEND=1
        BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_SAVE_TRACE=1
        BANISHED_FORCE_DEVICE_LOST=400
        BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120"

deploy() {
    cp -f "$HERE/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || return 1
    echo "== deployed md5 $(md5sum "$GAME_DIR/reimpl-x64.exe" | cut -d' ' -f1)"
    echo "== built    md5 $(md5sum "$HERE/standalone/build/reimpl-x64.exe" | cut -d' ' -f1)"
}

run_one() {
    local label="$1"; shift
    echo "===================== $label ====================="
    deploy || { echo "deploy FAILED"; return 1; }
    "$HERE/tools/rt_run.sh" "$label" --timeout 900 -- $COMMON "$@"
    local rc=$?
    echo "== rt_run rc=$rc"
    for f in standalone_debug.log standalone_error.log standalone_crash.log; do
        cp -f "$GAME_DIR/$f" "$OUT/${label}_$f" 2>/dev/null
    done
    ls -la "$OUT/${label}_standalone_crash.log" 2>/dev/null
    return $rc
}

run_one control
run_one armed "BANISHED_RT_MINT_ROWS=$ROWS"

echo "== batch complete; logs under $OUT"
