#!/usr/bin/env bash
# tools/drag_run.sh — Wave 10E lane DRAG.
#
# Modelled directly on lane MOUSE's tools/mouse_run.sh, which established this
# route. Everything is archived RAW: a truncated log manufactures the
# entries=0 signal this project reads ports on
# (reference_truncated_tool_output_manufactures_calls_zero.md).
#
#   tools/drag_run.sh <cell> [<cell> ...]
#
# Run it UNDER the lock, never around it:
#   tools/game_dir_lock.sh run lane-DRAG 1800 --snapshot ./lane_drag_logs -- \
#       tools/drag_run.sh drag_on drag_off
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OUT="$REPO/lane_drag_logs"
mkdir -p "$OUT"

if [ "${DRAG_NO_DEPLOY:-0}" != "1" ]; then
    echo "=== deploying $(stat -c %y "$REPO/standalone/build/reimpl-x64.exe")"
    echo "=== size: $(stat -c %s "$REPO/standalone/build/reimpl-x64.exe") bytes"
    cp -f "$REPO/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || exit 2
fi

run_cell() {
    local name="$1"; shift
    echo
    echo "############ CELL $name ############"
    echo "ENV: $*"
    "$REPO/tools/rt_run.sh" "$name" --timeout "${DRAG_TIMEOUT:-420}" -- "$@" \
        >"$OUT/$name.rt_run.txt" 2>&1
    local rc=$?
    cp -f "$GAME_DIR/standalone_error.log" "$OUT/$name.err.log" 2>/dev/null
    cp -f "$GAME_DIR/standalone_debug.log" "$OUT/$name.out.log" 2>/dev/null
    [ -s "$GAME_DIR/standalone_crash.log" ] && cp -f "$GAME_DIR/standalone_crash.log" "$OUT/$name.crash.log"
    echo "rc=$rc"
    echo "--- THE SUBJECT ROW (RAW) ---"
    grep -a 'BeginDragDrop' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- THE FIELD ORACLE: every field the function writes (OURS vs THEIRS) ---"
    grep -a 'DRAGFIELDS' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- the switch, and the registration count ---"
    grep -a -E 'UIDRAGSTART|\[RtPort\] UI: [0-9]+ exports' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- DOWNSTREAM ROWS: the A/B observable (must match across the switch) ---"
    grep -a -E '\[RtPort\] UI::(UserInterface::(IsInsideClipRegion|EndDragDrop|BeginToolTip))|\[RtPort\] UI::DialogInstance::EndHover' \
        "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- injector screens (dragStarted is the RATIO ANCHOR) ---"
    grep -a '\[MouseInject\]' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null | tail -40
    echo "--- crash ---"
    if [ -s "$OUT/$name.crash.log" ]; then head -20 "$OUT/$name.crash.log"; else echo "(none)"; fi
    echo "--- shutdown ---"
    grep -a 'Shutdown\] done' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null || echo "(no clean shutdown line)"
}

# Identical in-game budget to lane MOUSE's, so the two lanes' cells are
# comparable and the drag route is the one MOUSE proved.
INGAME="BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120"
DRAGSCRIPT="BANISHED_MOUSE_INJECT=hover,hover,off,hover,dropnow,off \
            BANISHED_MOUSE_TARGET=#time \
            BANISHED_RT_UIHOVER_DETOUR=1 \
            BANISHED_MOUSE_START=300 BANISHED_MOUSE_HOLD=40"

for cell in "$@"; do
case "$cell" in

  # -------- SUBJECT: the port armed (default-on kill switch) --------------
  # Identical to lane MOUSE's `all_det` cell in every respect. The only thing
  # that has changed since MOUSE ran it is that BeginDragDrop is now OUR body,
  # because the injector resolves it BY NAME and ResolveExport consults the
  # port registry first.
  drag_on)
    run_cell drag_on $INGAME $DRAGSCRIPT ;;

  # -------- NEGATIVE CONTROL: the same script, the port disarmed ----------
  # This is what makes the entry counter an instrument rather than an
  # assertion. The injector still calls BeginDragDrop exactly as often
  # (dragStarted is unchanged), but the call now lands in the DLL, so OUR
  # entries must read 0. entries 1 -> 0 while dragStarted holds at 1 is the
  # only pattern that says the counter is measuring THIS BODY and not the
  # injector's willingness to call something.
  #
  # It is also the A/B: every DOWNSTREAM row (EndDragDrop, EndHover,
  # BeginToolTip) must read IDENTICALLY across the two cells. Those rows are
  # the observable consequences of the drag our body started, and they are
  # produced by code this switch does not touch.
  drag_off)
    run_cell drag_off $INGAME $DRAGSCRIPT BANISHED_RT_UIDRAGSTART=0 ;;

  # -------- the no-injector control: is the row dead without a drag? ------
  # Establishes that the entries we count come from the injected drag and not
  # from ordinary gameplay -- retail's own UI never begins a drag on this route.
  noinject)
    run_cell noinject $INGAME ;;

  # -------- the ASYMMETRIC subject, and its two companions ----------------
  # The toolbar button is 40x40, so a centre grab is (20,20) and a TRANSPOSITION
  # of the two int32 stores is invisible. DRAGSKEW moves the aim point by an
  # asymmetric delta so grabX != grabY, which is what gives the field oracle
  # something to compare (reference_uniform_population_is_not_a_verification.md).
  skew_on)
    run_cell skew_on $INGAME $DRAGSCRIPT BANISHED_MOUSE_DRAGSKEW=3,-7 ;;
  skew_off)
    run_cell skew_off $INGAME $DRAGSCRIPT BANISHED_MOUSE_DRAGSKEW=3,-7 \
        BANISHED_RT_UIDRAGSTART=0 ;;

  # POSITIVE CONTROL: our body deliberately transposes the two grab offsets.
  # It MUST diverge from skew_off. If it does not, the oracle is blind and the
  # agreement in skew_on means nothing.
  skew_swap)
    run_cell skew_swap $INGAME $DRAGSCRIPT BANISHED_MOUSE_DRAGSKEW=3,-7 \
        BANISHED_DRAG_CONTROL=swap ;;

  *) echo "unknown cell '$cell'" ;;
esac
done
