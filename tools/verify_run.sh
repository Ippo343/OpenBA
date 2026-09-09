#!/usr/bin/env bash
# tools/verify_run.sh — WAVE 10E lane VERIFY.
#
# Same shape as tools/mouse_run.sh (lane MOUSE): deploy THIS worktree's debug
# exe once, then run the named cells in one lock hold, archiving everything RAW.
# A truncated log manufactures the entries=0 signal this project reads ports on.
#
#   tools/game_dir_lock.sh run lane-VERIFY 1800 --snapshot ./lane_verify_logs -- \
#       tools/verify_run.sh probes probes_ctl
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OUT="$REPO/lane_verify_logs"
mkdir -p "$OUT"

if [ "${VERIFY_NO_DEPLOY:-0}" != "1" ]; then
    echo "=== deploying $(stat -c %y "$REPO/standalone/build/reimpl-x64.exe")"
    cp -f "$REPO/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || exit 2
fi

run_cell() {
    local name="$1"; shift
    echo
    echo "############ CELL $name ############"
    echo "ENV: $*"
    "$REPO/tools/rt_run.sh" "$name" --timeout "${VERIFY_TIMEOUT:-420}" -- "$@" \
        >"$OUT/$name.rt_run.txt" 2>&1
    local rc=$?
    cp -f "$GAME_DIR/standalone_error.log" "$OUT/$name.err.log" 2>/dev/null
    cp -f "$GAME_DIR/standalone_debug.log" "$OUT/$name.out.log" 2>/dev/null
    [ -s "$GAME_DIR/standalone_crash.log" ] && cp -f "$GAME_DIR/standalone_crash.log" "$OUT/$name.crash.log"
    echo "rc=$rc"
    echo "--- probes ---"
    grep -a -E '\[ElemUpdateProbe\]|\[ElemPersistProbe\]|\[UICompCensus\]' \
        "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- tooltip family + element attendance ---"
    grep -a -E '\[RtPort\] UI::ToolTipDialog|\[Element\] ATTENDANCE|\[DlgSettings\] ATTENDANCE|\[RtPort\] UI::UserInterface::BeginToolTip' \
        "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- tipstr ---"
    grep -a -E '\[MouseInject/tipstr\]' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- injector screens ---"
    grep -a '\[MouseInject\]' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null | tail -20
    echo "--- crash ---"
    if [ -s "$OUT/$name.crash.log" ]; then head -20 "$OUT/$name.crash.log"; else echo "(none)"; fi
    echo "--- shutdown ---"
    grep -a 'Shutdown\] done' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null || echo "(no clean shutdown line)"
}

INGAME="BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120"

for cell in "$@"; do
case "$cell" in

  # ---- the two synthetic differentials. Menu only: both are self-contained
  #      and run from the IO probe block on the boot path, so nothing about
  #      gameplay is needed and a menu run is the cheapest place to score them.
  probes)
    run_cell probes BANISHED_AUTOQUIT=1 \
        BANISHED_RT_ELEMUPDATE_PROBE=1 BANISHED_RT_ELEMPERSIST_PROBE=1 ;;

  probes_ctl)
    run_cell probes_ctl BANISHED_AUTOQUIT=1 \
        BANISHED_RT_ELEMUPDATE_PROBE=1 BANISHED_RT_ELEMUPDATE_PROBE_NEGCTL=1 \
        BANISHED_RT_ELEMPERSIST_PROBE=1 BANISHED_RT_ELEMPERSIST_PROBE_NEGCTL=1 ;;

  # ---- the tooltip family's EMPTY arm, re-measured on this tip. This is the
  #      anchor cell: every ratio below is read against its operatorNew.
  #      The leading `probe` step dumps +0x58 for every live element.
  tip_empty)
    run_cell tip_empty $INGAME \
        BANISHED_MOUSE_INJECT=probe,hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_RT_UIHOVER_DETOUR=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # ---- the NON-EMPTY arm. Identical to tip_empty except that +0x58 is
  #      published through the real exported SetToolTipString before the hover.
  tip_nonempty)
    run_cell tip_nonempty $INGAME \
        BANISHED_MOUSE_INJECT=probe,hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_MOUSE_TIPSTR='lane VERIFY non-empty arm' \
        BANISHED_RT_UIHOVER_DETOUR=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # ---- does ANY live entity carry a UIComponent? The gate on OnOpened's
  #      settings half. Demo buildings widen the entity variety on purpose.
  uicomp)
    run_cell uicomp $INGAME \
        BANISHED_LIVE_TOOLS=1 BANISHED_TOOL_HARNESS=uicompcensus \
        BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_BUILD_DEMO_PASTURE=1 \
        BANISHED_BUILD_DEMO_ORCHARD=1 BANISHED_BUILD_DEMO_TRADINGPOST=1 ;;

  # ---- OnOpened's settings half: the census names the subject, then the
  #      three-select round trip drives badMagic and the record loop.
  dlgsettings)
    run_cell dlgsettings $INGAME \
        BANISHED_LIVE_TOOLS=1 BANISHED_TOOL_HARNESS=dlgsettings \
        BANISHED_DLGSETTINGS_ROUTE=256 \
        BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_BUILD_DEMO_PASTURE=1 \
        BANISHED_BUILD_DEMO_ORCHARD=1 BANISHED_BUILD_DEMO_TRADINGPOST=1 ;;

  # ---- the single-variable control for the cell above: the SAME three selects
  #      on the SAME subject, with the buffer install turned OFF. If `records`
  #      does not collapse here, the number above was not bought by the route.
  dlgsettings_ctl)
    run_cell dlgsettings_ctl $INGAME \
        BANISHED_LIVE_TOOLS=1 BANISHED_TOOL_HARNESS=dlgsettings \
        BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_BUILD_DEMO_PASTURE=1 \
        BANISHED_BUILD_DEMO_ORCHARD=1 BANISHED_BUILD_DEMO_TRADINGPOST=1 ;;

  *) echo "unknown cell '$cell'" ;;
esac
done
