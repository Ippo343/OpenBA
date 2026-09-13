#!/usr/bin/env bash
# tools/resave_gate/resave_gate.sh -- the load/resave byte-compare gate.
#
# Loads <save>.sav unattended (BANISHED_AUTOLOAD), triggers the retail autosave
# path at a fixed frame (BANISHED_SAVE_TEST), quits through the pause menu, and
# byte-compares the input save against the written Auto.sav. The DLL is moved
# aside for the run (DLL-absent is the only build from here on) and restored on
# every exit path.
#
# usage: tools/resave_gate/resave_gate.sh <save-name-without-.sav> [outdir] [seconds]
#   GATE_MODE=roundtrip (default): BANISHED_SAVE_TEST=roundtrip -- the in-process
#       check in gameupdateloop_serialize.cpp: right after the load restores the
#       world, serialize it again and compare the RAW bytes. Verdict is the
#       [Roundtrip] RESULT= line; both raw images are copied to <outdir>.
#   GATE_MODE=file: the whole-file compare (sim runs ~600 frames first, the
#       metadata block carries a thumbnail and a timestamp; expect DIFF -- kept
#       only as a smoke test that the DLL-absent save path completes).
set -u
MODE="${GATE_MODE:-roundtrip}"
G="/c/GOG Games/Banished"
S="$USERPROFILE/Documents/Banished/Save"
NAME="${1:?usage: resave_gate.sh <save-name> [outdir] [seconds]}"
OUT="${2:-docs/runtime_port/resave_gate/latest}"
SECS="${3:-400}"
EXE="standalone/build/reimpl-x64.exe"
mkdir -p "$OUT"

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[gate] Runtime-x64.dll restored"
  fi
}
trap restore EXIT INT TERM

[ -f "$S/$NAME.sav" ] || { echo "[gate] no such save: $S/$NAME.sav"; exit 2; }
cp -f "$EXE" "$G/reimpl-x64.exe" || { echo "[gate] exe busy, is a game running?"; exit 2; }
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" "$G/standalone_boot_state.txt"
rm -f "$S/Auto.sav"
mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
[ -f "$G/Runtime-x64.dll" ] && { echo "[gate] DLL still present, abort"; exit 2; }

cd "$G"
if [ "$MODE" = roundtrip ]; then
  SAVE_TEST=roundtrip; QUIT_FRAME="${GATE_QUIT_FRAME:-300}"
else
  SAVE_TEST="${GATE_SAVE_FRAME:-600}"; QUIT_FRAME="${GATE_QUIT_FRAME:-2400}"
fi
rm -f standalone_roundtrip_loaded.bin standalone_roundtrip_resaved.bin
env BANISHED_RT_NO_DLL=1 BANISHED_RT_ALLOC=1 BANISHED_RT_MINT=1 \
    BANISHED_MENU_SCENE="${GATE_MENU_SCENE:-0}" \
    BANISHED_AUTOLOAD="$NAME" BANISHED_SAVE_TEST="$SAVE_TEST" \
    BANISHED_PAUSEMENU="$QUIT_FRAME" BANISHED_PAUSEMENU_QUIT=120 \
    ./reimpl-x64.exe &
PID=$!
for i in $(seq 1 "$SECS"); do kill -0 $PID 2>/dev/null || break; sleep 1; done
if kill -0 $PID 2>/dev/null; then echo "[gate] alive after ${SECS}s, killing (HANG)"; kill -9 $PID; fi
wait $PID 2>/dev/null; RC=$?
cd - >/dev/null
echo "[gate] exit status: $RC"
for f in standalone_debug.log standalone_error.log standalone_crash.log; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[gate] SaveTest lines:"; grep -h "\[SaveTest\]" "$G/standalone_error.log" | grep -v "^\[SaveTest\]   " | head -20
echo "[gate] crash log bytes: $(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0)"

if [ "$MODE" = roundtrip ]; then
  for f in standalone_roundtrip_loaded.bin standalone_roundtrip_resaved.bin; do
    [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
  done
  LINE=$(grep -h "\[Roundtrip\] RESULT=" "$G/standalone_error.log" | tail -1)
  echo "[gate] in-process: $LINE"
  [ -f "$OUT/standalone_roundtrip_resaved.bin" ] || { echo "[gate] FAIL: no resaved image (load never reached the roundtrip check)"; exit 1; }
  # THE VERDICT IS AGAINST THE GOLDEN, not against the loaded image. Retail's
  # format numbers every RESIDENT resource into the reference table and writes
  # the InternalFactory table size, both of which depend on the process that
  # wrote the save, so loaded-vs-resaved is DIFF by design (see README.md).
  # With BANISHED_MENU_SCENE pinned the resave is byte-identical run to run,
  # so the resave of a known-good build is the reference.
  GOLD_DIR="docs/runtime_port/resave_gate/golden"; mkdir -p "$GOLD_DIR"
  GOLD="$GOLD_DIR/$NAME.resaved.bin"
  if [ ! -f "$GOLD" ]; then
    cp -f "$OUT/standalone_roundtrip_resaved.bin" "$GOLD"
    md5sum "$GOLD" | cut -c1-32 > "$GOLD_DIR/$NAME.resaved.md5"
    echo "[gate] GOLDEN CREATED: $GOLD ($(stat -c %s "$GOLD") bytes, md5 $(cat "$GOLD_DIR/$NAME.resaved.md5"))"
    echo "[gate] verdict: NEW-GOLDEN (re-run to get a PASS/DIFF)"; exit 0
  fi
  if cmp -s "$GOLD" "$OUT/standalone_roundtrip_resaved.bin"; then
    echo "[gate] verdict: PASS (resave == golden, $(stat -c %s "$GOLD") bytes)"; exit 0
  fi
  echo "[gate] verdict: DIFF vs golden -- $(cmp "$GOLD" "$OUT/standalone_roundtrip_resaved.bin" 2>&1 | head -1)"
  echo "[gate]   differing bytes: $(cmp -l "$GOLD" "$OUT/standalone_roundtrip_resaved.bin" 2>/dev/null | wc -l)  sizes: golden $(stat -c %s "$GOLD") now $(stat -c %s "$OUT/standalone_roundtrip_resaved.bin")"
  echo "[gate]   if the change is intended: rm $GOLD and re-run twice"
  exit 1
fi

if [ ! -f "$S/Auto.sav" ]; then echo "[gate] FAIL: no Auto.sav written"; exit 1; fi
cp -f "$S/Auto.sav" "$OUT/resaved.sav"
A="$S/$NAME.sav"; B="$S/Auto.sav"
echo "[gate] input  : $(stat -c %s "$A") bytes  $(md5sum "$A" | cut -c1-32)"
echo "[gate] resaved: $(stat -c %s "$B") bytes  $(md5sum "$B" | cut -c1-32)"
if cmp -s "$A" "$B"; then echo "[gate] PASS: byte-identical"; exit 0; fi
echo "[gate] DIFF: first differing byte: $(cmp "$A" "$B" 2>&1 | head -1)"
echo "[gate] differing bytes (cmp -l | wc -l): $(cmp -l "$A" "$B" 2>/dev/null | wc -l)"
exit 1
