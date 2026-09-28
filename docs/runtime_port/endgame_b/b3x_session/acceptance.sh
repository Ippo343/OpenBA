#!/usr/bin/env bash
# B3X_RULES.md section 6 -- the acceptance of the normal-virtual cutover, from the repo root.
#   bash docs/runtime_port/endgame_b/b3x_session/acceptance.sh
# Every count is over CODE lines (codegrep.py: comments stripped, string literals kept).
# The exempt set is B3X_RULES X1: the twelve Video::*Data classes (runtime/types/video/CompanionData.h,
# their callers in runtime/src/video and runtime/src/save / engine, dx11's seam) and, in tests/, their
# mirrors plus the ONE control class of object_vtable_abi_test.cpp and slot_check's own stand-in.
# EDIT WITH THE Write TOOL ONLY.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
CG="python $HERE/codegrep.py"
n() { grep -vc '^# '; }
line() { printf '%-4s %-74s %-6s target %s\n' "$1" "$2" "$3" "$4"; }
PROD="code runtime standalone engine dx11"

echo "== B3X acceptance @ $(git rev-parse --short HEAD) $(date +%Y-%m-%d\ %H:%M)"
line X1 "DeletingDtor( declarations / definitions, product" \
  "$($CG '\bDeletingDtor\s*\(\s*(unsigned|uint32_t|int)\b' $PROD | n)" "12 (the *Data classes)"
line X1b "  ... of them outside runtime/types/video/CompanionData.h" \
  "$($CG '\bDeletingDtor\s*\(\s*(unsigned|uint32_t|int)\b' $PROD | grep -v 'runtime/types/video/CompanionData.h' | n)" 0
line X2 "DeletingDtor(0) calls, product" \
  "$($CG 'DeletingDtor\s*\(\s*0u?\s*\)' $PROD | n)" 0
line X3 "DeletingDtor(1) calls, product" \
  "$($CG 'DeletingDtor\s*\(\s*1u?\s*\)' $PROD | n)" "8 (*Data receivers)"
echo "     receivers:"; $CG 'DeletingDtor\s*\(\s*1u?\s*\)' $PROD | grep -v '^# ' | cut -c1-150 | sed 's/^/       /'
line X4 "Object::Destroy( calls, product" \
  "$($CG '\bObject::Destroy\s*\(' $PROD | n)" 0
line X5 "Object::Destroy is delete (runtime/types/object/TypeInfo.h)" \
  "$($CG 'inline\s+void\s+Destroy\s*\(\s*Base\s*\*\s*p\s*\)\s*\{\s*delete\s+p\s*;' runtime/types/object | n)" 1
line X6 "CallSlot0( anywhere" "$($CG '\bCallSlot0\s*\(' $PROD tests | n)" 0
line X7 "per-file PMF decoders in tests (PmfSlot)" "$($CG '\bPmfSlot\w*\s*[<(]' tests | n)" 0
line X8 "static_assert(!has_virtual_destructor<Video::*Data>) pins" \
  "$($CG 'static_assert\s*\(\s*!\s*std::has_virtual_destructor' runtime/types/video | n)" 12
line X9 "DeletingDtor in tests (mirrors of exempt classes, the control, slot_check)" \
  "$($CG '\bDeletingDtor\b' tests | n)" "listed in W1_TESTS_DONE.md"
echo "     files: $($CG '\bDeletingDtor\b' tests | grep -v '^# ' | cut -d: -f1 | sort | uniq -c | tr '\n' ' ')"
line X10 "global operator delete replaced" \
  "$($CG '^\s*void\s+operator\s+delete\s*\(' $PROD | n)" 0
line X11 "root operator delete definitions (root_operator_delete.cpp)" \
  "$($CG '::operator\s+delete\s*\(\s*void\s*\*' runtime/src/object/root_operator_delete.cpp | n)" 5
echo "X12  classes.py --summary:"
python "$HERE/classes.py" --summary 2>/dev/null | head -12 | sed 's/^/       /'
echo "X13  partc.py (slot-index residue; every product line is classified in W0_AUDIT.md):"
python "$HERE/partc.py" | sed -E 's/  \[.*//' | sed 's/^/       /'
