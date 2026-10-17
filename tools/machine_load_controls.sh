#!/usr/bin/env bash
# =============================================================================
# tools/machine_load_controls.sh -- does machine_load_report.py actually read
# the file, or does it print a verdict regardless?
# =============================================================================
#
# `CLEAN` is an ABSENCE verdict, and an absence screen has no natural positive
# case: a parser that reads nothing at all also prints "nothing found". This
# project has paid for that shape repeatedly, so every cell here HANDS the
# reporter a shape and requires it to name that shape.
#
# The cells are chosen so each corrupts something the reporter actually reads:
#   CLEAN         nothing on the watched list -> CLEAN
#   CONTAM_EXE    a second reimpl-x64.exe     -> CONTAMINATED (the worst case)
#   CONTAM_GAME   another game binary         -> CONTAMINATED
#   BUSY_COMPILE  compilers during the run    -> BUSY, and NOT contaminated
#   BUSY_CPU      high CPU only               -> BUSY
#   PY_BASELINE   3 pythons BEFORE and DURING -> CLEAN. This is the mirror
#                 control: a screen that flagged the baseline's own processes
#                 would report BUSY here and would be wrong in the direction
#                 that MANUFACTURES work.
#   EMPTY         header only                 -> UNEXAMINED, never CLEAN
#   NOLOAD        what --no-load writes       -> UNEXAMINED, never CLEAN
#   MISSING       no file at all              -> UNEXAMINED, never CLEAN
#
# EMPTY and MISSING are the dead-parser cells. If either reads CLEAN, every
# other CLEAN in this project's perf results is unfalsifiable.
# =============================================================================
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RPT="$HERE/machine_load_report.py"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/mlr.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

PASS=0; FAIL=0
report() {
    if [ "$2" = "0" ]; then echo "PASS -- $1"; PASS=$((PASS+1))
    else echo "FAIL -- $1"; echo "       $3"; FAIL=$((FAIL+1)); fi
}

HDR='t_s,cpu_pct,free_mb,procs,make,compiler,python,reimpl,other_game'

# cell <label> <expect-verdict> <must-mention regex|-> <csv-body...>
cell() {
    local label="$1" expect="$2" mention="$3"; shift 3
    local f="$TMP/$label.csv" out got
    printf '%s\n' "$HDR" > "$f"
    local line
    for line in "$@"; do printf '%s\n' "$line" >> "$f"; done
    out=$(python "$RPT" "$f" 2>&1)
    got=$(printf '%s' "$out" | sed -n 's/^machine load: \([A-Z]*\).*/\1/p' | tail -n 1)
    if [ "$got" != "$expect" ]; then
        report "$label: expected $expect" 1 "got '$got'; output:
$out"
        return
    fi
    if [ "$mention" != "-" ] && ! printf '%s' "$out" | grep -qi -- "$mention"; then
        report "$label: verdict $expect but the reason was not named" 1 \
            "expected the output to mention '$mention'; output:
$out"
        return
    fi
    # Every verdict must state what it examined -- a verdict with no subject is
    # this project's most repeated instrument defect.
    if [ "$expect" != "UNEXAMINED" ] && ! printf '%s' "$out" | grep -q 'examined [0-9]* samples'; then
        report "$label: the verdict does not say what it examined" 1 "output:
$out"
        return
    fi
    report "$label: $expect" 0 ""
}

echo "== machine_load_report.py controls =="
echo "subject: $RPT"
echo

#        label          expect        must mention              rows (t_s,cpu,mem,procs,make,comp,py,reimpl,othergame)
cell CLEAN        CLEAN        -                 '-1,20,8000,300,0,0,1,0,0' '0,30,7900,301,0,0,1,1,0' '5,28,7880,301,0,0,1,1,0'
cell CONTAM_EXE   CONTAMINATED 'SECOND reimpl'   '-1,20,8000,300,0,0,1,0,0' '0,30,7900,302,0,0,1,2,0' '5,31,7880,302,0,0,1,2,0'
cell CONTAM_GAME  CONTAMINATED 'another game'    '-1,20,8000,300,0,0,1,0,0' '0,30,7900,302,0,0,1,1,1'
cell BUSY_COMPILE BUSY         'compiler ran'    '-1,20,8000,300,0,0,1,0,0' '0,66,6000,320,1,4,1,1,0' '5,60,6000,320,1,4,1,1,0'
cell BUSY_CPU     BUSY         'median CPU'      '-1,20,8000,300,0,0,1,0,0' '0,95,7000,300,0,0,1,1,0' '5,92,7000,300,0,0,1,1,0'
# MIRROR CONTROL: the pythons were ALREADY there. Flagging them would be an
# inflating blind spot, and an inflating screen MANUFACTURES work.
cell PY_BASELINE  CLEAN        -                 '-1,20,8000,300,0,0,3,0,0' '0,30,7900,301,0,0,3,1,0' '5,29,7880,301,0,0,3,1,0'

# ---- the dead-parser cells --------------------------------------------------
cell EMPTY        UNEXAMINED   'no samples'
# --no-load writes the header plus this comment line, so the report section
# still EXISTS and says nothing was measured. A results dir with no
# machine_load.csv at all would be silently absent, and a silently absent load
# record reads exactly like a quiet machine.
cell NOLOAD       UNEXAMINED   'NOT'              '# --no-load was passed: NOTHING WAS SAMPLED. This is not a quiet machine.'

out=$(python "$RPT" "$TMP/does_not_exist.csv" 2>&1)
if printf '%s' "$out" | grep -q 'UNEXAMINED'; then
    report "MISSING: a file that does not exist reads UNEXAMINED, not CLEAN" 0 ""
else
    report "MISSING: a file that does not exist must read UNEXAMINED" 1 "output:
$out"
fi

echo
echo "== summary (per-cell, not aggregate) =="
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
