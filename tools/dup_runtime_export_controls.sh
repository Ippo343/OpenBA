#!/usr/bin/env bash
# tools/dup_runtime_export_controls.sh -- controls for dup_runtime_export.py.
#
# WHY: Wave 20 Phase 0c taught that checker to separate a SAME-TARGET duplicate
# (a no-op) from a DIFFERENT-TARGET one (a shadowed body), and gave it an
# allowlist. Both changes can only make it quieter. A checker that was made
# quieter and then reports "0 DEFECT" has to prove it can still fail.
#
# The two strongest cases here are not synthetic: C2 and C3 re-introduce the
# ACTUAL defect text, pulled out of git, that this phase removed from the tree.
#
# SUBJECT: the dup_runtime_export.py sitting next to this script (printed).
# EXIT: 0 if every applicable case behaved, 1 otherwise.

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOL="$SCRIPT_DIR/dup_runtime_export.py"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
FIX="${TMPDIR:-/tmp}/dup_ctl_$$"
mkdir -p "$FIX" || exit 1
trap 'rm -rf "$FIX"' EXIT

echo "SUBJECT (tool): $TOOL"
echo "SUBJECT (repo): $REPO"
echo "FIXTURES      : $FIX"

PASS=0; FAIL=0; TOTAL=0

# Never read an exit code through a pipe. Capture, then test.
run_on() {   # $1 = root
    LAST_OUT="$(python "$TOOL" --root "$1" 2>&1)"
    return $?
}

expect() {   # $1=label $2=root $3=rc $4=grep-must-match ("" = skip)
    TOTAL=$(( TOTAL + 1 ))
    run_on "$2"; local rc=$?
    local ok=1
    [ "$rc" = "$3" ] || ok=0
    if [ -n "$4" ] && ! printf '%s' "$LAST_OUT" | grep -q "$4"; then ok=0; fi
    if [ "$ok" = "1" ]; then
        PASS=$(( PASS + 1 )); echo "  PASS  $1  (rc=$rc, expected $3)"
    else
        FAIL=$(( FAIL + 1 )); echo "  FAIL  $1  (rc=$rc, expected $3, wanted /$4/)"
        printf '%s\n' "$LAST_OUT" | sed 's/^/        | /'
    fi
}

mkfix() {   # $1 = name -> echoes its root
    local d="$FIX/$1"; mkdir -p "$d/runtime/src"; touch "$d/.git"; echo "$d"
}

echo
echo "=== NEGATIVE: the tree as it stands must be clean ==="
expect "C5 real repo -> 0 DEFECT" "$REPO" 0 "0 DEFECT"

echo
echo "=== POSITIVE: the two REAL defects this phase removed, restored from git ==="
echo "    (not synthetic -- 'git show HEAD:<file>' of the pre-fix source)"

D="$(mkfix renderbuffer)"
git -C "$REPO" show HEAD:runtime/src/video/buffers.cpp          > "$D/runtime/src/buffers.cpp"
git -C "$REPO" show HEAD:runtime/src/video/context_lifecycle.cpp > "$D/runtime/src/context_lifecycle.cpp"
expect "C2 RenderBuffer::PlatformDestruct two bodies -> DEFECT" "$D" 3 \
       "PlatformDestruct@RenderBuffer"

D="$(mkfix input)"
git -C "$REPO" show HEAD:runtime/src/input/input.cpp                > "$D/runtime/src/input.cpp"
git -C "$REPO" show HEAD:runtime/src/input/input_config_startup.cpp > "$D/runtime/src/input_config_startup.cpp"
expect "C3 0x1800e0c80 split brain -> DEFECT" "$D" 3 "0x1800e0c80"

echo
echo "=== POSITIVE: a fresh duplicate on a key nobody allowlisted ==="
D="$(mkfix synthetic)"
cat > "$D/runtime/src/a.cpp" <<'EOF'
void reg() {
    RegisterRuntimeExport("?Totally@New@Symbol@@QEAAXXZ", (void*)&BodyOne);
    RegisterRuntimeAddr(0x180099999, (void*)&BodyOne);
}
EOF
cat > "$D/runtime/src/b.cpp" <<'EOF'
void reg2() {
    RegisterRuntimeExport("?Totally@New@Symbol@@QEAAXXZ", (void*)&BodyTwo);
    RegisterRuntimeAddr(0x180099999, reinterpret_cast<void*>(&BodyTwo));
}
EOF
expect "C1 two bodies, one new key -> DEFECT" "$D" 3 "Totally@New@Symbol"

echo
echo "=== NEGATIVE: the two quieting changes must not swallow everything ==="
D="$(mkfix sametarget)"
cat > "$D/runtime/src/a.cpp" <<'EOF'
void reg() { RegisterRuntimeAddr(0x180088888, (void*)&TheOnlyBody); }
EOF
cat > "$D/runtime/src/b.cpp" <<'EOF'
void reg2() { RegisterRuntimeAddr(0x180088888, reinterpret_cast<void*>(&TheOnlyBody)); }
EOF
expect "C4 same target through different casts -> no defect" "$D" 0 "1 same-target"

echo "--- C6: does an ALLOWLISTED KEY still get its duplicate reported at all?"
D="$(mkfix allowlisted)"
cat > "$D/runtime/src/a.cpp" <<'EOF'
void reg() {
    RegisterRuntimeExport("?Free@MemoryAllocator@System@@QEAAXPEAX@Z", (void*)&Impostor);
    RegisterRuntimeExport("?Free@MemoryAllocator@System@@QEAAXPEAX@Z", (void*)&Other);
}
EOF
expect "C6 allowlisted key, UNRELATED bodies -> STILL rc 0 (a KNOWN limit)" "$D" 0 "MATCHED"

echo "        ^ This is a LIMITATION, recorded rather than papered over: the"
echo "          allowlist is keyed by SYMBOL, so once a key is exempted, a NEW"
echo "          and genuinely bad duplicate on that same key is exempted too."
echo "          Ten keys are exempt. Widening an entry to (key, both sites) would"
echo "          close it and is the obvious next edit; it is not done here because"
echo "          it was not measured against the real tree."

echo
echo "=== The subject must be named BEFORE any finding ==="
TOTAL=$(( TOTAL + 1 ))
FIRST="$(python "$TOOL" --root "$REPO" 2>&1 | head -1)"
if printf '%s' "$FIRST" | grep -q '^SUBJECT ROOT:'; then
    PASS=$(( PASS + 1 )); echo "  PASS  C7 first line is the subject: $FIRST"
else
    FAIL=$(( FAIL + 1 )); echo "  FAIL  C7 first line was: $FIRST"
fi

echo
echo "=== A git WORKTREE must not trip the 'no .git' warning ==="
TOTAL=$(( TOTAL + 1 ))
run_on "$REPO"
if printf '%s' "$LAST_OUT" | grep -q 'no .git here'; then
    FAIL=$(( FAIL + 1 )); echo "  FAIL  C8 warned 'no .git' inside a real worktree"
else
    PASS=$(( PASS + 1 )); echo "  PASS  C8 no false '.git' warning in a worktree (.git is a FILE there)"
fi

echo
echo "======================================================================"
echo "APPLICABLE CASES: $TOTAL   PASS: $PASS   FAIL: $FAIL"
echo "  positive (must exit 3): C1, C2, C3      <- C2/C3 are the REAL defects"
echo "  negative (must exit 0): C4, C5, C6"
echo "  hygiene               : C7 subject-first, C8 worktree .git"
echo "======================================================================"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
