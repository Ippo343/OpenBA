#!/usr/bin/env bash
# Coordinator gate for ENDGAME B.3.4 merges. Run from the repo root (main).
#   gate.sh <label> [resave-runs=3] [mode]
#     mode: ""            builds + checks + three routes + resave
#           skipbuild     checks + three routes + resave (exe already built; build logs, if
#                         any, are read from runs/<label>/build_*.log)
#           resaveonly    resave runs only (exe already built)
#           warnbaseline  regenerate the warning-set baseline from runs/<label>/build_*.log
#                         (ONLY from a true `make clean` rebuild: an incremental build prints
#                         warnings only for what recompiled, and a baseline cut from one made
#                         23 standing warnings look new at block 1)
# The summary ends with "GATE RESULT: n problem(s)".
#
# READING A RESAVE DIFF: reftable_diff.py names the resident-resource rows that differ from the
# golden and says whether they explain the WHOLE size change. RACE-SHAPED = re-runnable (the
# loader race: tools/resave_gate/README.md). REAL-SHAPED = localise it, do not re-run it away.
# The tool never turns a DIFF into a PASS: acceptance is 3 CONSECUTIVE PASSes (B34_RULES 3.4).
#
# READING A ROUTE: exit 4 + hang log with the loader thread in RtIo_CompressedStream_Read, or ONE
# boot with topics=-1 / diseases=-1 and openCompFail>=1, is the pre-existing loader defect
# (coord NOTES.md FINDING 1): re-run the route. The same resource NULL on every boot is real.
#
# EDIT THIS FILE WITH THE Write TOOL ONLY. A python/heredoc edit through the Bash tool
# collapses doubled backslashes and once turned a sed back-reference into a control char.
set -u
LABEL="${1:?label}"; RESAVE_N="${2:-3}"; MODE="${3:-}"
MK=/c/msys64/ucrt64/bin/mingw32-make.exe
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/$LABEL"; mkdir -p "$OUT"
BASE="$HERE/runs/baseline"
GOLD="docs/runtime_port/resave_gate/golden/Gate.resaved.bin"
PROBLEMS=0

# One warning per (file, message): line:col dropped so an edit that shifts lines is not
# "new". Log paths are already repo-relative. No backslashes in this expression on purpose.
norm() { grep 'warning:' "$1" | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | LC_ALL=C sort -u; }

if [ "$MODE" = warnbaseline ]; then
  for c in debug perf; do
    norm "$OUT/build_standalone-$c.log" > "$HERE/runs/warn_baseline_$c.txt"
    echo "warn baseline $c: $(wc -l < "$HERE/runs/warn_baseline_$c.txt") entries from $(grep -c -E -- ' -c ' "$OUT/build_standalone-$c.log") compiled files (runs/$LABEL)"
  done
  exit 0
fi

SUM="$OUT/summary.txt"; : > "$SUM"
say() { echo "$*" | tee -a "$SUM"; }
bad() { PROBLEMS=$((PROBLEMS+1)); say "  !! $*"; }
count() { cat "$E" "$D" 2>/dev/null | grep -c -- "$1"; }   # one number, no bc

warncheck() {
  local c neww
  for c in debug perf; do
    [ -f "$OUT/build_standalone-$c.log" ] || { say "  warnings ($c): no build log in runs/$LABEL"; continue; }
    norm "$OUT/build_standalone-$c.log" > "$OUT/warn_$c.txt"
    LC_ALL=C comm -23 "$OUT/warn_$c.txt" "$HERE/runs/warn_baseline_$c.txt" > "$OUT/warn_new_$c.txt"
    neww=$(wc -l < "$OUT/warn_new_$c.txt")
    say "  new warnings ($c, vs clean-rebuild baseline set): $neww"
    if [ "$neww" -ne 0 ]; then
      head -15 "$OUT/warn_new_$c.txt" | tee -a "$SUM"
      [ "$neww" -gt 15 ] && say "  ... $((neww-15)) more in runs/$LABEL/warn_new_$c.txt (in $(sed -E 's/: warning:.*//' "$OUT/warn_new_$c.txt" | LC_ALL=C sort -u | wc -l) files)"
      bad "new warnings in $c build"
    fi
  done
}

say "== gate $LABEL @ $(git rev-parse --short HEAD) $(date +%H:%M:%S) mode=${MODE:-full}"
if [ -z "$MODE" ]; then
  for t in standalone-debug standalone standalone-perf unit; do
    $MK -j4 $t > "$OUT/build_$t.log" 2>&1; rc=$?
    say "build $t: exit $rc"
    [ $rc -ne 0 ] && { tail -30 "$OUT/build_$t.log" | tee -a "$SUM"; say "GATE RESULT: build failed"; exit 1; }
  done
fi
if [ "$MODE" != resaveonly ]; then
  # `standalone` (release) SHARES standalone/build/obj with standalone-debug, so after the
  # debug build it compiles nothing: a no-op relink check, NOT a release compile.
  # -DNDEBUG / log-strip coverage comes from standalone-perf (own object dir).
  [ -f "$OUT/build_standalone.log" ] && say "  note: release target compiled $(grep -c -E -- ' -c ' "$OUT/build_standalone.log") files (shared obj dir; perf is the real NDEBUG build)"
  warncheck
  if [ -f "$OUT/build_unit.log" ]; then
    say "unit: $(grep -E '[0-9]+ tests' "$OUT/build_unit.log" | tail -1)"
    grep -qE ' 0 failures' "$OUT/build_unit.log" || bad "unit failures"
  fi
  say "dispatch: $(python tools/dispatch_slot_map.py --check 2>&1 | tail -1)"
  say "manifest: $(python tools/cutover_manifest.py --gate 2>&1 | tail -1)"
  say "mintgen: $(python tools/rt_mint_gen.py --check 2>&1 | tail -1)"
fi

tools/game_dir_lock.sh acquire coord 1800 || { say "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord' EXIT

route() { # name secs env...
  local name="$1" secs="$2"; shift 2
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || { bad "route $name: exe busy"; return 1; }
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env "$@" ./reimpl-x64.exe ) & local pid=$!
  local i; for i in $(seq 1 "$secs"); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  if kill -0 $pid 2>/dev/null; then bad "route $name: ALIVE after ${secs}s, killing"; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; fi
  wait $pid 2>/dev/null; local rc=$?
  mkdir -p "$OUT/$name"
  for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do
    [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$name/$f"
  done
  local crash; crash=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0)
  local hang=no; [ -f "$G/standalone_hang.log" ] && hang=YES
  say "route $name: exit $rc crashlog=$crash hang=$hang (${i}s)"
  if [ "$rc" -ne 0 ] || [ "$crash" -ne 0 ] || [ "$hang" = YES ]; then bad "route $name not clean"; fi
  E="$OUT/$name/standalone_error.log"; D="$OUT/$name/standalone_debug.log"
  local trap_n warn_n; trap_n=$(count '\[RtMint\] TRAP'); warn_n=$(count 'GATE WARN')
  say "  RtMint TRAP=$trap_n  GATE WARN=$warn_n  $(cat "$E" "$D" 2>/dev/null | grep '\[RtResolve\]' | grep -i miss | tail -1)"
  say "  loader: $(cat "$E" "$D" 2>/dev/null | grep '\[ExtLoad\] calls=' | tail -1 | sed -E 's/.*(calls=[0-9]+) (openFileFail=[0-9]+) (openCompFail=[0-9]+).*(retNull=[0-9]+).*/\1 \2 \3 \4/')"
  [ "$trap_n" -ne 0 ] && bad "route $name: RtMint TRAP"
  [ "$warn_n" -ne 0 ] && bad "route $name: GATE WARN"
  cat "$E" "$D" 2>/dev/null | grep -q '\[RtResolve\] 0 misses' || bad "route $name: no '[RtResolve] 0 misses' line"
}

if [ "$MODE" != resaveonly ]; then
  route newgame 600 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
  cat "$E" "$D" 2>/dev/null | grep -q 'OnCommand\] -> App::Quit' || bad "newgame never reached the pause-menu Quit"
  say "  $(cat "$E" "$D" | grep 'Post-Startup _firstTypeInfo' | tail -1)"
  say "  $(cat "$E" "$D" | grep 'Object::factory dtors' | tail -1 | cut -c1-70)"
  # Pointer-free, order-free type table: the registered count, the chain count, every
  # id/name pair the boot prints, every lazy name->id resolve, and the two list counts.
  cat "$E" "$D" | grep -E '\[FactoryReg\] Registered|Post-Startup _firstTypeInfo|\[GameMain\]   typeId=|ResolveTypeIdByNameLazy|\[GUL::DiseaseList\] gw|\[GUL::Help\] gw' \
    | sed -E 's/[0-9A-Fa-f]{16}/PTR/g' | LC_ALL=C sort -u > "$OUT/typetable.txt"
  say "  typetable: $(wc -l < "$OUT/typetable.txt") lines md5 $(md5sum "$OUT/typetable.txt" | cut -c1-8)"
  if [ -f "$BASE/typetable.txt" ] && [ "$LABEL" != baseline ]; then
    if diff -q "$BASE/typetable.txt" "$OUT/typetable.txt" >/dev/null; then say "  typetable == baseline"
    else
      # B.3.8: by-name lookups converted to X::TypeId() drop their ResolveTypeIdByNameLazy log
      # line. A diff made ONLY of removed lazy-lookup lines is the same table.
      local_extra=$(diff "$BASE/typetable.txt" "$OUT/typetable.txt" | grep -E '^[<>]' | grep -v -E '^< \[FactoryReg\] ResolveTypeIdByNameLazy' | wc -l)
      local_gone=$(diff "$BASE/typetable.txt" "$OUT/typetable.txt" | grep -c -E '^< \[FactoryReg\] ResolveTypeIdByNameLazy')
      if [ "$local_extra" -eq 0 ]; then say "  typetable == baseline minus $local_gone lazy by-name lookup line(s) (now X::TypeId())"
      else bad "typetable differs from baseline"; diff "$BASE/typetable.txt" "$OUT/typetable.txt" | head -20 | tee -a "$SUM"; fi
    fi
  fi
  route tutorial 600 BANISHED_AUTOTUTORIAL=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
  route loadgame 300 BANISHED_AUTOMENU_BUTTON=2 BANISHED_AUTOMENU_CLICKS=m6
fi

PASSN=0; STREAK=0
for n in $(seq 1 "$RESAVE_N"); do
  tools/resave_gate/resave_gate.sh Gate "$OUT/resave$n" > "$OUT/resave$n.log" 2>&1; rc=$?
  NEW="$OUT/resave$n/standalone_roundtrip_resaved.bin"
  if [ $rc -eq 0 ]; then PASSN=$((PASSN+1)); STREAK=$((STREAK+1)); say "resave $n: PASS ($(stat -c %s "$NEW" 2>/dev/null) bytes)"
  else
    STREAK=0
    say "resave $n: exit $rc"
    if [ -f "$NEW" ]; then python "$HERE/reftable_diff.py" "$GOLD" "$NEW" 2>&1 | sed 's/^/    /' | tee -a "$SUM"
    else say "    no resaved image (load never reached the roundtrip check): $(tail -2 "$OUT/resave$n.log" | tr '\n' ' ')"; fi
    bad "resave $n DIFF"
  fi
  grep -q 'crash log bytes: 0' "$OUT/resave$n.log" || bad "resave $n: crash log not empty"
  rm -f "$OUT/resave$n"/*.bin
done
say "resave: $PASSN/$RESAVE_N PASS, trailing consecutive PASS streak = $STREAK (B34_RULES 3.4 wants 3 consecutive)"
say "GATE RESULT: $PROBLEMS problem(s)   $(date +%H:%M:%S)"
[ "$PROBLEMS" -eq 0 ]
