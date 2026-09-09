#!/usr/bin/env bash
# tools/mouse_run.sh — Wave 10D lane MOUSE.
#
# Deploys THIS worktree's debug exe once, then runs the named cells in one lock
# hold. Everything is archived RAW: a truncated log manufactures the calls=0 /
# entries=0 signal this project reads ports on
# (reference_truncated_tool_output_manufactures_calls_zero.md).
#
#   tools/mouse_run.sh <cell> [<cell> ...]
#
# Run it UNDER the lock, never around it:
#   tools/game_dir_lock.sh run lane-MOUSE 1800 --snapshot ./lane_mouse_logs -- \
#       tools/mouse_run.sh baseline probe
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OUT="$REPO/lane_mouse_logs"
mkdir -p "$OUT"

if [ "${MOUSE_NO_DEPLOY:-0}" != "1" ]; then
    echo "=== deploying $(stat -c %y "$REPO/standalone/build/reimpl-x64.exe")"
    cp -f "$REPO/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || exit 2
fi

run_cell() {
    local name="$1"; shift
    echo
    echo "############ CELL $name ############"
    echo "ENV: $*"
    "$REPO/tools/rt_run.sh" "$name" --timeout "${MOUSE_TIMEOUT:-420}" -- "$@" \
        >"$OUT/$name.rt_run.txt" 2>&1
    local rc=$?
    cp -f "$GAME_DIR/standalone_error.log" "$OUT/$name.err.log" 2>/dev/null
    cp -f "$GAME_DIR/standalone_debug.log" "$OUT/$name.out.log" 2>/dev/null
    [ -s "$GAME_DIR/standalone_crash.log" ] && cp -f "$GAME_DIR/standalone_crash.log" "$OUT/$name.crash.log"
    echo "rc=$rc"
    echo "--- THE THREE ROWS + the clip control (RAW) ---"
    grep -a -E '\[RtPort\] UI::(UserInterface::(IsInsideClipRegion|EndDragDrop|BeginToolTip))|\[RtPort\] UI::DialogInstance::EndHover' \
        "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- injector screens ---"
    grep -a '\[MouseInject\]' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null | tail -40
    echo "--- crash ---"
    if [ -s "$OUT/$name.crash.log" ]; then head -20 "$OUT/$name.crash.log"; else echo "(none)"; fi
    echo "--- shutdown ---"
    grep -a 'Shutdown\] done' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null || echo "(no clean shutdown line)"
}

# The in-game frame budget every cell shares. 900 frames of gameplay, then the
# pause menu drives a clean Quit Game -- the pausemenu route, which lane SHOW
# established already does in-game AND clean-quit together.
INGAME="BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120"

for cell in "$@"; do
case "$cell" in

  # -------- the BEFORE measurement: injector never armed -------------------
  baseline)
    run_cell baseline $INGAME ;;

  # -------- what does the in-game UI actually offer? -----------------------
  probe)
    run_cell probe $INGAME \
        BANISHED_MOUSE_INJECT=probe BANISHED_MOUSE_START=300 ;;

  # -------- the subject: hover -> tooltip -> leave -------------------------
  # `hover` holds long enough for BeginToolTip; `off` parks the cursor outside
  # every rect, which is what makes Element::Update take its EndHover arm.
  tooltip)
    run_cell tooltip $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # -------- same, but on a TOOLBAR button, with the dwell trace on ---------
  tooltip_tb)
    run_cell tooltip_tb $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' BANISHED_MOUSE_TRACE=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # -------- THE SUBJECT CELL: injector + the opt-in hover detours ----------
  # Without the detour the DLL's own Element::Update takes the tooltip arm and
  # calls the DLL's bodies; with it, the same genuine hover reaches ours.
  tooltip_det)
    run_cell tooltip_det $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_RT_UIHOVER_DETOUR=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # The detour ALONE, no injector: the control that shows the detour by itself
  # buys nothing, which is exactly why UIF6 was right to reject it in Wave 10C.
  det_only)
    run_cell det_only $INGAME BANISHED_RT_UIHOVER_DETOUR=1 ;;

  # -------- the subject: a real drag, dropped on an element ----------------
  drag)
    run_cell drag $INGAME \
        BANISHED_MOUSE_INJECT=hover,drag,drop,off \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=30 ;;

  drag_det)
    run_cell drag_det $INGAME \
        BANISHED_MOUSE_INJECT=hover,dropnow,off \
        BANISHED_MOUSE_TARGET='#time' BANISHED_MOUSE_TRACE=1 \
        BANISHED_RT_UIHOVER_DETOUR=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # -------- everything, one run: the headline before/after ----------------
  all_det)
    run_cell all_det $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,dropnow,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_RT_UIHOVER_DETOUR=1 \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # -------- both, in one run ----------------------------------------------
  both)
    run_cell both $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,drag,drop,off \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 ;;

  # -------- the four negative controls, each scored on ALL five screens ----
  # The first three use the SAME target as the subject cell, so the only thing
  # that differs from a passing run is the injected defect. `ctl_notip` must
  # NOT name a target: its whole defect is "aim at an element that is not
  # tooltip-capable", and a `#time` selector would override that and make the
  # control vacuous (a control that cannot fire must not be scored as passing).
  ctl_nowrite)
    run_cell ctl_nowrite $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 \
        BANISHED_MOUSE_CONTROL=nowrite ;;
  ctl_badscale)
    run_cell ctl_badscale $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 \
        BANISHED_MOUSE_CONTROL=badscale ;;
  ctl_offtarget)
    run_cell ctl_offtarget $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_TARGET='#time' \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 \
        BANISHED_MOUSE_CONTROL=offtarget ;;
  ctl_notip)
    run_cell ctl_notip $INGAME \
        BANISHED_MOUSE_INJECT=hover,hover,off,hover,off \
        BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40 \
        BANISHED_MOUSE_CONTROL=notip ;;

  # -------- the modal self-test lane UIF6 fixed; must still PASS -----------
  # NOTE: this one writes its verdict to standalone_error.log, NOT the debug
  # log. Grepping the debug log returns nothing and reads exactly like "the
  # driver never armed" -- it fooled the integrator once.
  modal)
    run_cell modal BANISHED_AUTOQUIT=1 \
        BANISHED_MODAL_DRIVER=2 BANISHED_MODAL_DRIVER_TEST=1 ;;

  *) echo "unknown cell '$cell'" ;;
esac
done
