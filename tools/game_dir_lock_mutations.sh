#!/usr/bin/env bash
# =============================================================================
# tools/game_dir_lock_mutations.sh -- does the lock selftest actually CATCH
# anything, or does it only ever pass?
# =============================================================================
#
# WHY THIS EXISTS
#
# tools/game_dir_lock_selftest.sh is a screen, and this project has paid
# repeatedly for screens nobody handed a positive case:
#
#   * "a screen looking for the ABSENCE of a shape must be HANDED one -- it has
#     no natural positive case, so 0 hits is indistinguishable from a DEAD
#     PARSER."
#   * "a control must corrupt something the check actually compares." And its
#     mirror: "a control that corrupts a byte the probe compares but SHOULD NOT
#     reports CAUGHT and exonerates a broken instrument."
#
# Defect 4's cells are exactly that shape: five of them assert that a lock was
# NOT broken. A `break_if_stale` that had been accidentally turned into `return
# 1` would pass all five. So this script MUTATES the subject -- one defect per
# mutation, each reintroducing a specific real failure -- and requires the
# suite to fail the named cells and ONLY those cells.
#
# The mutations are not hypothetical. M1 IS the Wave 25 code that cost lane
# ALLOC a measurement pair. M3 IS the silence ALLOC reported. M4/M5 are the two
# ways the identity check can be wrong in each direction.
#
# USAGE
#   tools/game_dir_lock_mutations.sh          # all mutations
#   tools/game_dir_lock_mutations.sh M2       # one
#
# Touches no game directory: the selftest already runs entirely in scratch
# dirs, and the mutated subjects live in a temp dir that is removed after.
# =============================================================================
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUBJECT="$HERE/game_dir_lock.sh"
SUITE="$HERE/game_dir_lock_selftest.sh"
WANT="${1:-}"

PASS=0; FAIL=0
report() {
    local label="$1" ok="$2" detail="$3"
    if [ "$ok" = "0" ]; then echo "PASS -- $label"; PASS=$((PASS+1))
    else echo "FAIL -- $label"; echo "       $detail"; FAIL=$((FAIL+1)); fi
}

# Run the suite against a mutated subject; echo the labels of the FAILING cells,
# one per line.
failing_cells() {
    local mutated="$1"
    GAMELOCK_SELFTEST_SUBJECT="$mutated" bash "$SUITE" 2>/dev/null \
        | sed -n 's/^FAIL -- //p'
}

# A mutation is verified when the set of failing cells is EXACTLY the expected
# set. Not a count -- a count that moves the right direction by the wrong
# amount reads as success, and this project has a recorded instance of that
# hiding a phantom. The SET is diffed.
check_mutation() {
    local name="$1" desc="$2" expect_re="$3" mutated="$4"
    local got missed extra line
    # A mutation whose anchor no longer matches writes NO file, and the suite
    # would then run against a path that does not exist and fail every cell --
    # which reads as "caught!" if you only look at the count. Refuse to score
    # a mutation that was never applied.
    if [ ! -s "$mutated" ]; then
        report "$name ($desc)" 1 "the mutation was NOT APPLIED (no subject written -- its anchor no longer matches the source). Nothing was tested."
        return
    fi
    got="$(failing_cells "$mutated")"

    if [ -z "$got" ]; then
        report "$name ($desc)" 1 "the suite passed CLEAN against a subject with this defect reintroduced -- the screen is dead"
        return
    fi
    missed=""
    # every expected pattern must appear among the failures
    local pat
    while IFS= read -r pat; do
        [ -z "$pat" ] && continue
        printf '%s\n' "$got" | grep -q -- "$pat" || missed="${missed}${missed:+; }$pat"
    done <<< "$expect_re"
    # and nothing unexpected may
    extra=""
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        local ok=1
        while IFS= read -r pat; do
            [ -z "$pat" ] && continue
            case "$line" in *"$pat"*) ok=0 ;; esac
        done <<< "$expect_re"
        [ "$ok" = 1 ] && extra="${extra}${extra:+; }$line"
    done <<< "$got"

    if [ -n "$missed" ]; then
        report "$name ($desc)" 1 "these cells should have FAILED and did not: $missed
actual failures:
$got"
        return
    fi
    if [ -n "$extra" ]; then
        report "$name ($desc)" 1 "unexpected collateral failures (the mutation should be surgical): $extra"
        return
    fi
    report "$name ($desc) -- caught by exactly the expected cells" 0 ""
}

TMP="$(mktemp -d "${TMPDIR:-/tmp}/gdl_mut.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

# ---------------------------------------------------------------------------
# M1 -- the Wave 25 code: staleness on the wall clock alone.
# ---------------------------------------------------------------------------
# Must break the four cells that assert a LIVE holder survives, and must NOT
# break "dead holder must still break" (the old code broke that one too) nor
# "under the floor" (the old code kept it too). That asymmetry is the whole
# point: a mutation that fails everything proves nothing about aim.
m1() {
    local f="$TMP/m1.sh"
    python - "$SUBJECT" "$f" <<'PY'
import io,sys
src,dst=sys.argv[1],sys.argv[2]
s=io.open(src,encoding='utf-8',newline='').read()
old='    verdict=$(lock_staleness)'
new='    verdict=$(lock_staleness >/dev/null; if [ "$(lock_age)" -gt "$STALE_AFTER" ]; then echo STALE; else echo YOUNG; fi)'
assert old in s, "M1 anchor missing"
io.open(dst,'w',encoding='utf-8',newline='').write(s.replace(old,new))
PY
    check_mutation M1 "staleness decided by the wall clock alone (the Wave 25 code)" \
"defect 4/live exe pid
defect 4/recent write
defect 4/handles held open
defect 4/fresh holder heartbeat" "$f"
}

# ---------------------------------------------------------------------------
# M2 -- the opposite error: never break anything.
# ---------------------------------------------------------------------------
# The mutation the five KEEP cells CANNOT catch on their own. If only M1 were
# run, a subject that deadlocks every lane behind a corpse would score 15/15.
m2() {
    local f="$TMP/m2.sh"
    python - "$SUBJECT" "$f" <<'PY'
import io,sys
src,dst=sys.argv[1],sys.argv[2]
s=io.open(src,encoding='utf-8',newline='').read()
old='    verdict=$(lock_staleness)'
new='    verdict=$(lock_staleness >/dev/null; echo ALIVE)'
assert old in s, "M2 anchor missing"
io.open(dst,'w',encoding='utf-8',newline='').write(s.replace(old,new))
PY
    check_mutation M2 "a lock is never stale (deadlock forever)" \
"defect 4/dead holder must still break
defect 4/hard ceiling breaks despite every live signal
defect 5" "$f"
}

# ---------------------------------------------------------------------------
# M3 -- ALLOC's actual experience: the break happens, silently.
# ---------------------------------------------------------------------------
m3() {
    local f="$TMP/m3.sh"
    python - "$SUBJECT" "$f" <<'PY'
import io,sys
src,dst=sys.argv[1],sys.argv[2]
s=io.open(src,encoding='utf-8',newline='').read()
old='    record_breach "$owner" "$age" "$breaker" "$verdict"'
assert old in s, "M3 anchor missing"
io.open(dst,'w',encoding='utf-8',newline='').write(s.replace(old,'    : # breach record suppressed (M3)'))
PY
    check_mutation M3 "the lock is broken and nothing records it (ALLOC got no signal)" \
"defect 5" "$f"
}

# ---------------------------------------------------------------------------
# M4 -- the identity watcher flags EVERY change, including an ordinary deploy.
# ---------------------------------------------------------------------------
# An over-sensitive screen. It catches the real clobber, so a suite that only
# tested the clobber would call it correct -- and every lane would learn to
# ignore its warning, which is worse than no warning.
m4() {
    local f="$TMP/m4.sh"
    python - "$SUBJECT" "$f" <<'PY'
import io,sys
src,dst=sys.argv[1],sys.argv[2]
s=io.open(src,encoding='utf-8',newline='').read()
old='if [ -n "$prev" ] && [ "$cur" != "$prev" ] && [ "$prev_live" = 1 ]; then'
new='if [ -n "$prev" ] && [ "$cur" != "$prev" ]; then'
assert old in s, "M4 anchor missing"
io.open(dst,'w',encoding='utf-8',newline='').write(s.replace(old,new))
PY
    check_mutation M4 "the identity watcher flags an ordinary deploy as a clobber" \
"defect 6/exe replaced while idle" "$f"
}

# ---------------------------------------------------------------------------
# M5 -- the identity watcher never flags anything.
# ---------------------------------------------------------------------------
m5() {
    local f="$TMP/m5.sh"
    python - "$SUBJECT" "$f" <<'PY'
import io,sys
src,dst=sys.argv[1],sys.argv[2]
s=io.open(src,encoding='utf-8',newline='').read()
old='if [ -n "$prev" ] && [ "$cur" != "$prev" ] && [ "$prev_live" = 1 ]; then'
new='if false; then'
assert old in s, "M5 anchor missing"
io.open(dst,'w',encoding='utf-8',newline='').write(s.replace(old,new))
PY
    check_mutation M5 "the identity watcher never notices a clobber" \
"defect 6/exe replaced WHILE the run is live" "$f"
}

echo "== game_dir_lock mutation controls =="
echo "subject: $SUBJECT"
echo "suite:   $SUITE"
echo

# The baseline. If the UNMUTATED subject does not pass clean, every result
# below is meaningless -- a mutation's "caught" would be the baseline's own
# failure wearing the mutation's name.
base="$(failing_cells "$SUBJECT")"
if [ -n "$base" ]; then
    echo "ABORT: the unmutated subject already fails these cells:"
    echo "$base"
    exit 1
fi
echo "baseline: the unmutated subject passes every cell"
echo

for m in M1 M2 M3 M4 M5; do
    [ -n "$WANT" ] && [ "$WANT" != "$m" ] && continue
    case "$m" in
        M1) m1 ;; M2) m2 ;; M3) m3 ;; M4) m4 ;; M5) m5 ;;
    esac
done

echo
echo "== summary (per-mutation, not aggregate) =="
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
