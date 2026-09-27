#!/usr/bin/env bash
# Coordinator: merge a block branch into main, regenerate the mint on a generated-file conflict, gate.
# usage: mergegate.sh <branch> <label> "<merge title>" [ui] [tools]      EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
HERE="$(cd "$(dirname "$0")" && pwd)"
BR="$1"; LABEL="$2"; TITLE="$3"; shift 3
PY=tools/ghidra_mcp/.venv/Scripts/python.exe
cd "$REPO" || exit 2
git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
[ -n "$(git status --porcelain)" ] && { echo "TREE NOT CLEAN"; git status --short | head; exit 2; }
PRE=$(git rev-parse --short HEAD)
printf '%s\n\nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>\n' "$TITLE" > "$HERE/mergemsg.txt"
if ! git merge --no-ff --no-commit "$BR" > "$HERE/merge_$LABEL.log" 2>&1; then
  CONF=$(git diff --name-only --diff-filter=U)
  echo "conflicts: $CONF"
  for f in $CONF; do
    case "$f" in
      standalone/rt_data_mint_tables.inc|standalone/cutover_manifest.inc)
        git checkout --theirs -- "$f"; git add "$f";;
      *) echo "UNRESOLVED CONFLICT in $f — merge left in progress (pre-merge HEAD $PRE)"; exit 3;;
    esac
  done
fi
$PY tools/rt_mint_gen.py > "$HERE/mintgen_$LABEL.log" 2>&1 || { echo "MINT GENERATOR REFUSED:"; tail -20 "$HERE/mintgen_$LABEL.log"; echo "merge left in progress (pre-merge HEAD $PRE)"; exit 4; }
git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
git add -A standalone/rt_data_mint_tables.inc
git commit -q -F "$HERE/mergemsg.txt" || { echo "commit failed"; exit 5; }
echo "merged $BR: $PRE -> $(git rev-parse --short HEAD)"
bash "$HERE/gate.sh" "$LABEL" 5 > "$HERE/gate_$LABEL.out" 2>&1
grep -E "^==|^build|new warnings|^unit:|mintgen|^route|loader:|typetable|^resave|GATE RESULT|!!|^[<>]" "$HERE/runs/$LABEL/summary.txt" | cut -c1-230
git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
if grep -q "GATE RESULT: 0 problem" "$HERE/runs/$LABEL/summary.txt"; then
  for x in "$@"; do
    [ "$x" = ui ] && { bash "$HERE/uiroutes.sh" "${LABEL}_ui" > /dev/null 2>&1; cat "$HERE/runs/${LABEL}_ui/summary.txt" | cut -c1-230; }
    [ "$x" = tools ] && { bash "$HERE/toolroutes.sh" "${LABEL}_tools" 2>&1 | grep -E '^tool ' | cut -c1-200; }
  done
fi
N="$HERE/runs/$LABEL/newgame"
cat "$N"/standalone_error.log "$N"/standalone_debug.log 2>/dev/null | grep -E '^\[RtMint\] (minted|registered)|^\[RtDelDtor\]|FACTREST registered|FACTUI' | cut -c1-200 | head -8
git status --short | head -5
