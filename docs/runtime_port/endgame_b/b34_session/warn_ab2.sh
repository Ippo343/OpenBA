#!/bin/bash
# Same A/B as warn_ab.sh, but the BASE copy is materialised NEXT TO the real file
# so that quoted same-directory #includes resolve identically on both sides.
# Run from the worktree root.  Usage: bash warn_ab2.sh <base-ref> <file>...
BASE="$1"; shift
COMMON="-std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type -Wno-invalid-offsetof -I. -Iengine/types -Iengine/funcs -Icode/Game -Iruntime/types -Iruntime/funcs -Idx11/types -Idx11/funcs -Istandalone"
DBG="$COMMON -g -O0 -D_DEBUG"
PERF="$COMMON -O2 -g -DNDEBUG -DBANISHED_PERF_EXPERIMENTAL=1 -DBANISHED_STRIP_LOGS -include standalone/log_strip.h"
rc=0
for f in "$@"; do
  d=$(dirname "$f"); b=$(basename "$f" .cpp)
  tmp="$d/__abbase_${b}.cpp"
  git show "$BASE:$f" > "$tmp" || { echo "$b (new file)"; continue; }
  for mode in DBG PERF; do
    flags=${!mode}
    ao=$(g++ $flags -fsyntax-only "$tmp" 2>&1)
    if echo "$ao" | grep -q "fatal error"; then echo "$b [$mode] BASE COMPILE FAILED"; rc=2; continue; fi
    a=$(echo "$ao" | grep -c "warning:")
    no=$(g++ $flags -fsyntax-only "$f" 2>&1)
    n=$(echo "$no" | grep -c "warning:")
    echo "$b [$mode]  base=$a  new=$n"
    if [ "$n" -gt "$a" ]; then rc=1; echo "   --- NEW ---"; echo "$no" | grep "warning:"; fi
  done
  rm -f "$tmp"
done
exit $rc
