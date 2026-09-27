#!/usr/bin/env bash
# B.3.8 owed step-close items + the B.3.9 baseline, in one pass on a clean tree.
#   1. true release compile from a clean obj dir, then make clean
#   2. clean rebuild debug/perf/unit -> runs/baseline/build_*.log, warnbaseline
#   3. gate.sh baseline 5 skipbuild (routes + resave x5 + mintgen/dispatch/manifest) = the clean-built exe's gate
#   4. stub-set diff vs step 8's baseline new-game log, tutorial census line
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/owed"; mkdir -p "$OUT"
SUM="$OUT/summary.txt"; : > "$SUM"
MK=/c/msys64/ucrt64/bin/mingw32-make.exe
J="${J:-10}"
say() { echo "$*" | tee -a "$SUM"; }
cd "$REPO" || exit 2
say "== owed @ $(git rev-parse --short HEAD) $(date +%H:%M:%S)"

$MK clean > "$OUT/clean1.log" 2>&1
$MK -j$J standalone > "$OUT/build_release.log" 2>&1; rc=$?
say "true release compile: exit $rc, $(grep -c -E -- ' -c ' "$OUT/build_release.log") files compiled, exe $(stat -c %s standalone/build/reimpl-x64.exe 2>/dev/null) bytes, warnings $(grep -c 'warning:' "$OUT/build_release.log"), errors $(grep -c 'error' "$OUT/build_release.log")"
$MK clean > "$OUT/clean2.log" 2>&1

BL="$HERE/runs/baseline"; mkdir -p "$BL"
for t in standalone-debug standalone-perf unit; do
  $MK -j$J $t > "$BL/build_$t.log" 2>&1; rc=$?; say "clean rebuild $t: exit $rc ($(grep -c -E -- ' -c ' "$BL/build_$t.log") files)"
  [ $rc -ne 0 ] && { tail -20 "$BL/build_$t.log" | tee -a "$SUM"; say "OWED RESULT: build failed"; exit 1; }
done
bash "$HERE/gate.sh" baseline 3 warnbaseline | tee -a "$SUM"
for c in debug perf; do
  say "warnings $c: new vs step-8 baseline set: $(LC_ALL=C comm -23 "$HERE/runs/warn_baseline_$c.txt" "$HERE/runs/step8_baseline/warn_baseline_$c.txt" | wc -l)"
done
bash "$HERE/gate.sh" baseline 5 skipbuild > "$OUT/gate_baseline.log" 2>&1; rc=$?
say "gate baseline exit $rc"
cat "$BL/summary.txt" | tee -a "$SUM"

NG="$BL/newgame"; B8="$HERE/runs/step8_baseline/newgame"
stub() { cat "$1/standalone_error.log" "$1/standalone_debug.log" 2>/dev/null | grep 'NOT IMPLEMENTED' | sed -E 's/0x[0-9A-Fa-f]{8,}/ADDR/g; s/[0-9A-Fa-f]{16}/PTR/g' | LC_ALL=C sort -u; }
stub "$NG" > "$OUT/stubset_now.txt"; stub "$B8" > "$OUT/stubset_base.txt"
say "NOT IMPLEMENTED hits on new-game: $(cat "$NG/standalone_error.log" "$NG/standalone_debug.log" 2>/dev/null | grep -c 'NOT IMPLEMENTED')"
say "stub set: $(LC_ALL=C comm -13 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | wc -l) new, $(LC_ALL=C comm -23 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | wc -l) gone vs step-8 baseline"
LC_ALL=C comm -13 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | head -10 | sed 's/^/  NEW /' | tee -a "$SUM"
say "typetable vs step-8 baseline: $(diff "$HERE/runs/step8_baseline/typetable.txt" "$BL/typetable.txt" | grep -E '^[<>]' | grep -v -c -E '^< \[FactoryReg\] ResolveTypeIdByNameLazy') non-lazy line(s) differ"
TU="$BL/tutorial"
say "tutorial census: $(cat "$TU/standalone_error.log" "$TU/standalone_debug.log" 2>/dev/null | grep -h 'TownCensus\] LOAD exit' | head -1)"
git status --short | head -5 | tee -a "$SUM"
say "== done $(date +%H:%M:%S)"
