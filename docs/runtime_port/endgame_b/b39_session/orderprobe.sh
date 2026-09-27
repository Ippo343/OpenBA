#!/usr/bin/env bash
# B38_RULES section 6 item 2: type-table order sensitivity. Throwaway worktree, four debug builds.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
HERE="$(cd "$(dirname "$0")" && pwd)"
WT="$REPO/.claude/worktrees/order"
P=/c/Users/USPC/AppData/Local/Temp/claude/b39/0C
G="/c/GOG Games/Banished"
MK=/c/msys64/ucrt64/bin/mingw32-make.exe
OUT="$HERE/runs/orderprobe"; mkdir -p "$OUT"
SUM="$OUT/summary.txt"; : > "$SUM"
say() { echo "$*" | tee -a "$SUM"; }
cd "$REPO" || exit 2
[ -d "$WT" ] || git worktree add --detach "$WT" HEAD > "$OUT/wt.log" 2>&1 || { say "worktree failed"; exit 2; }
mkdir -p "$WT/docs/runtime_port/resave_gate/golden"
cp -f docs/runtime_port/resave_gate/golden/Gate.resaved.bin "$WT/docs/runtime_port/resave_gate/golden/"
cd "$WT" || exit 2
say "== orderprobe @ $(git rev-parse --short HEAD) $(date +%H:%M:%S)"
for v in dump A B C; do
  case $v in dump) pf=order_probe_dump.patch;; A) pf=order_probe.patch;; B) pf=order_probe_mint.patch;; C) pf=order_probe_dup.patch;; esac
  git checkout -q -- . ; git apply "$P/$pf" || { say "$v: patch failed"; continue; }
  $MK -j10 standalone-debug > "$OUT/build_$v.log" 2>&1; rc=$?
  say "$v: build exit $rc"
  [ $rc -ne 0 ] && { grep -E 'error' "$OUT/build_$v.log" | head -10 | tee -a "$SUM"; continue; }
  tools/game_dir_lock.sh acquire coord 1800 || { say "lock failed"; exit 1; }
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe ) & pid=$!
  for i in $(seq 1 600); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  kill -0 $pid 2>/dev/null && taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1
  wait $pid 2>/dev/null; rc=$?
  mkdir -p "$OUT/$v"
  for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$v/$f"; done
  tools/game_dir_lock.sh release coord
  L="$OUT/$v"
  cat "$L"/standalone_error.log "$L"/standalone_debug.log | grep '^\[OrderProbe\] ' | LC_ALL=C sort > "$OUT/tree_$v.txt"
  cat "$L"/standalone_error.log "$L"/standalone_debug.log | grep '^\[OrderProbeChain\]' > "$OUT/chain_$v.txt"
  cat "$L"/standalone_error.log "$L"/standalone_debug.log | grep -E '\[FactoryReg\] Registered|Post-Startup _firstTypeInfo|\[GameMain\]   typeId=|ResolveTypeIdByNameLazy|\[GUL::DiseaseList\] gw|\[GUL::Help\] gw' | sed -E 's/[0-9A-Fa-f]{16}/PTR/g' | LC_ALL=C sort -u > "$OUT/typetable_$v.txt"
  say "$v: newgame exit $rc crash=$(stat -c %s "$L/standalone_crash.log" 2>/dev/null || echo 0) (${i}s) TRAP=$(cat "$L"/*.log | grep -c '\[RtMint\] TRAP') $(cat "$L"/*.log | grep '\[ExtLoad\] calls=' | tail -1 | sed -E 's/.*(calls=[0-9]+) (openFileFail=[0-9]+).*/\1 \2/')"
  say "$v: tree lines $(wc -l < "$OUT/tree_$v.txt") md5 $(md5sum "$OUT/tree_$v.txt" | cut -c1-8); chain lines $(wc -l < "$OUT/chain_$v.txt") md5 $(md5sum "$OUT/chain_$v.txt" | cut -c1-8); typetable md5 $(md5sum "$OUT/typetable_$v.txt" | cut -c1-8)"
  say "$v: $(cat "$L"/*.log | grep 'Post-Startup _firstTypeInfo' | tail -1)"
  for n in 1 2; do
    tools/resave_gate/resave_gate.sh Gate "$OUT/resave_${v}_$n" > "$OUT/resave_${v}_$n.log" 2>&1; rc=$?
    say "$v: resave $n exit $rc $(grep -E 'verdict|crash log bytes' "$OUT/resave_${v}_$n.log" | tr '\n' ' ')"
    rm -f "$OUT/resave_${v}_$n"/*.bin
  done
  if [ $v != dump ]; then
    say "$v: tree diff vs dump: $(diff "$OUT/tree_dump.txt" "$OUT/tree_$v.txt" | grep -c '^[<>]') line(s); typetable diff: $(diff "$OUT/typetable_dump.txt" "$OUT/typetable_$v.txt" | grep -c '^[<>]') line(s)"
    diff "$OUT/tree_dump.txt" "$OUT/tree_$v.txt" | head -8 | cut -c1-220 | tee -a "$SUM"
    diff "$OUT/typetable_dump.txt" "$OUT/typetable_$v.txt" | grep '^[<>]' | grep -v 'typeId=' | head -5 | tee -a "$SUM"
  fi
done
git checkout -q -- .
say "== done $(date +%H:%M:%S)"
