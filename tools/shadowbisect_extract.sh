#!/usr/bin/env bash
# Extract the lines that matter from every archived cell log, in the order a
# reader must take them: armed/skip counters and the gap detector BEFORE
# divergences.
set -u
cd "$(dirname "$0")/.."
D=docs/runtime_port/shadowbisect_logs
for f in "$@"; do
  echo "================ cell $f ================"
  grep -E "LEGACY CONTROL|NEGATIVE CONTROL|GAP DETECTOR|ENTRY \(min|COVERAGE|GROWTH grow|GAPS checks|gaps=0 over|SUMMARY armed|MEASURED NOTHING|HARNESS DEFECT" "$D/${f}_error.log" 2>/dev/null
  echo "---- first allocator divergence ----"
  grep -E "DIVERGENCE System::MemoryAllocator" "$D/${f}_error.log" 2>/dev/null | head -2
  echo "---- first gaps ----"
  grep -E "GAP #[123] " "$D/${f}_error.log" 2>/dev/null | head -3
done
