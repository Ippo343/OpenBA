#!/usr/bin/env bash
# Coordinator: finish a merge that mergegate.sh left IN PROGRESS after the conflicts were resolved by
# hand: regenerate the generated files, commit with the given title, gate.
# usage: finishmerge.sh <label> "<merge title>" [ui] [tools]      EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
HERE="$(cd "$(dirname "$0")" && pwd)"
LABEL="$1"; TITLE="$2"; shift 2
PY=tools/ghidra_mcp/.venv/Scripts/python.exe
cd "$REPO" || exit 2
for f in standalone/rt_data_mint_tables.inc; do   # (B.3.9 close: the cutover manifest is deleted)
  git diff --name-only --diff-filter=U | grep -qx "$f" && { git checkout --theirs -- "$f"; git add "$f"; }
done
LEFT=$(git diff --name-only --diff-filter=U)
[ -n "$LEFT" ] && { echo "STILL UNRESOLVED: $LEFT"; exit 3; }
grep -rlE '^(<<<<<<<|>>>>>>>) ' $(git diff --cached --name-only | grep -E '\.(cpp|h|py|inc|md|txt)$') 2>/dev/null | head -3 | grep . && { echo "CONFLICT MARKERS LEFT"; exit 3; }
[ ! -f tools/rt_mint_gen.py ] || [ ! -f standalone/rt_data_mint_tables.inc ] || $PY tools/rt_mint_gen.py > "$HERE/mintgen_$LABEL.log" 2>&1 || { echo "MINT GENERATOR REFUSED:"; grep -E "STILL READ|REFUS" -A3 "$HERE/mintgen_$LABEL.log" | head -20; exit 4; }
git ls-files --error-unmatch standalone/rt_export_addrs.inc >/dev/null 2>&1 && git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
[ -f standalone/rt_data_mint_tables.inc ] && git add -A standalone/rt_data_mint_tables.inc
printf '%s\n\nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>\n' "$TITLE" > "$HERE/mergemsg.txt"
git commit -q -F "$HERE/mergemsg.txt" || { echo "commit failed"; exit 5; }
echo "merged: $(git log --oneline -1 | cut -c1-120)"
bash "$HERE/gate.sh" "$LABEL" 5 > "$HERE/gate_$LABEL.out" 2>&1
grep -E "^==|^build|new warnings|^unit:|mintgen|^route|loader:|registry:|stub set|RtUnresolved|typetable|^resave:|GATE RESULT|!!|^[<>]" "$HERE/runs/$LABEL/summary.txt" | cut -c1-200
git ls-files --error-unmatch standalone/rt_export_addrs.inc >/dev/null 2>&1 && git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
if grep -q "GATE RESULT: 0 problem" "$HERE/runs/$LABEL/summary.txt"; then
  for x in "$@"; do
    [ "$x" = ui ] && { bash "$HERE/uiroutes.sh" "${LABEL}_ui" > /dev/null 2>&1; cut -c1-200 "$HERE/runs/${LABEL}_ui/summary.txt"; }
    [ "$x" = tools ] && { bash "$HERE/toolroutes.sh" "${LABEL}_tools" 2>&1 | grep -E '^tool ' | cut -c1-120; }
    [ "$x" = devlost ] && { DLFRAME=40000 DLPAUSE=48000 bash "$HERE/devlost.sh" "${LABEL}_devlost" 2>&1 | grep devlost; }
  done
fi
git status --short | head -5
