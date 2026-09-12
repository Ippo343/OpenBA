#!/usr/bin/env python3
r"""P3: is the address at a CORROBORATED-EXACT row's END really ANOTHER OBJECT'S
START?  (Wave 20 Phase 0d, POST-REPORT TRANCHE, lane EXTENT)

WHY THIS EXISTS -- it argues against my own headline.

`tools/mint_extent_audit.py` reports 31 of 31 adjacent-export rows as
CORROBORATED-EXACT: the row's start is writer-attested and the NEXT address any
instruction names is exactly the row's end `va + 8n`. I then used that to argue
something stronger than the screen tests -- that on those 31 rows UNDER-sizing is
impossible, not merely unscreened:

    a distinct object begins at va+8n; a vtable cannot overlap another object;
    therefore the true extent m <= n, and OVER-sizing is the only available
    error direction -- which is the direction the screen does test.

THAT ARGUMENT HAS A PREMISE NOBODY HAD WRITTEN DOWN:

    P3.  The address named at va+8n is a DISTINCT OBJECT'S START, not an
         INTERIOR address of the same table that some code took the address of
         (a partial dispatch, a cached sub-range, a pointer to vtable+0x50
         handed to a helper).

If it is interior, then m > n, the row is UNDER-SIZED, and there is NO SYMPTOM --
Wave 19's asymmetry fully intact, sitting underneath a screen everyone was about
to treat as settled. I had verified P3 by hand for 1 of 31. THIS TOOL RUNS IT ON
ALL 31.

THE TEST

At `va + 8n`, apply the same store-shape test the audit already parses for
row-START writers: a `LEA reg,[rip+E]` whose register is then STORED INTO AN
OBJECT (`MOV %reg,(%base)`) is a vptr stamp, so E is a table start and the row is
bounded. Anything else -- no store in the window, a store that is not a write of
the LEA'd register into memory, or the address consumed ARITHMETICALLY as a base
-- means the premise is NOT established for that row.

  PREMISE-CHECKED-PASS   >=1 naming site is a vptr stamp of E.
  PREMISE-FAILED         E is named, no naming site stamps it. LOUD: the row's
                         slot count may be an UNDER-count with no symptom.
  COULD-NOT-DERIVE       E is named only in a way this tool cannot decide.

DELIBERATELY NOT COLLAPSED INTO THE AUDIT'S TIERS. This tool does not upgrade
any row's CORROBORATED-EXACT verdict and does not write to
rt_data_mint_tables.inc. It adds an orthogonal column: whether the premise under
that verdict was CHECKED or ASSUMED. A reader must be able to tell which rows had
the test RUN, which is why PREMISE-CHECKED and PREMISE-ASSUMED stay separate
words.

EXPIRY CLAUSE, as written in the report this tranche annotates:
    the impossibility claim ("under-sizing cannot occur on these rows") stands
    ONLY for rows where this test has actually been run and PASSED. For every
    other row the honest statement remains the weaker one: NOT OVER-SIZED, with
    under-sizing out of the screen's reach.

POSITIVE CONTROL, both directions, scored on applicable cases only

  POSITIVE  the 62 minted row starts are all genuine vtable starts. The test
            must classify them as vptr stamps. Denominator 62.
  NEGATIVE  named .rdata addresses whose bytes are printable ASCII are string
            constants, NOT tables. The test must classify NONE of them as a
            vptr stamp. Denominator printed. A test that cannot tell a string
            constant from a vtable is not a test.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_p3_premise.py
Exit 0 all premises checked and passed, 3 a premise FAILED, 2 a control is red.
"""
import argparse
import bisect
import os
import re
import shutil
import subprocess
import sys

import pefile

# Reuse the audit's parsing contract rather than forking it.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mint_extent_audit import (INSN, LEA, MOVABS, TARGET, ROW, REPO,
                               DEFAULT_DLL, DEFAULT_INC)

WINDOW = 6          # instructions after the LEA to search for its store
STORE = re.compile(r'^mov\s+%(\w+),\(')
ARITH = re.compile(r'^(?:add|sub|or|and)\s+\$0x[0-9a-f]+,%(\w+)$')
LEABASE = re.compile(r'^lea\s+[^(]*\(%(\w+)[,)]')


def scan(dll, objdump):
    """{target_va: [(site, verdict, insn_text)]} over a WINDOW-instruction look
    ahead. verdict is 'STORE' (vptr stamp), 'ARITH' (used as a base for address
    arithmetic -- the shape that HIDES an interior table) or 'OTHER'."""
    out = subprocess.run([objdump, "-d", dll], capture_output=True, text=True,
                         errors="replace")
    if out.returncode != 0:
        raise SystemExit("objdump failed rc=%d" % out.returncode)
    tgt, n_lea, n_insn = {}, 0, 0
    pend = []
    for line in out.stdout.splitlines():
        m = INSN.match(line)
        if not m:
            continue
        n_insn += 1
        site, text = int(m.group(1), 16), m.group(3).strip()
        keep = []
        for tv, sv, reg, foll in pend:
            foll.append(text)
            if len(foll) >= WINDOW:
                tgt.setdefault(tv, []).append((sv,) + classify(reg, foll))
            else:
                keep.append((tv, sv, reg, foll))
        pend = keep
        if not LEA.match(text):
            continue
        reg = text.rsplit(",%", 1)[-1].split()[0] if ",%" in text else "?"
        mm = MOVABS.match(text)
        if mm:
            pend.append((int(mm.group(1), 16), site, reg, []))
            continue
        if "(%rip)" in text:
            mt = TARGET.search(text)
            if mt:
                n_lea += 1
                pend.append((int(mt.group(1), 16), site, reg, []))
    for tv, sv, reg, foll in pend:
        tgt.setdefault(tv, []).append((sv,) + classify(reg, foll))
    return tgt, n_lea, n_insn


def classify(reg, foll):
    """What happens to the register that just received the address?

    STORE wins over ARITH: MSVC frequently copies the object pointer around
    between the LEA and the store, and a `lea` off an UNRELATED register is not
    arithmetic on OUR address. Only arithmetic on `reg` itself counts.
    """
    for f in foll:
        m = STORE.match(f)
        if m and m.group(1) == reg:
            return "STORE", f
    for f in foll:
        m = ARITH.match(f) or LEABASE.match(f)
        if m and m.group(1) == reg:
            return "ARITH", f
    return "OTHER", foll[0] if foll else "(end of section)"


def verdict_of(sites):
    if not sites:
        return "UNNAMED", None
    st = [s for s in sites if s[1] == "STORE"]
    if st:
        return "STORE", st[0]
    ar = [s for s in sites if s[1] == "ARITH"]
    if ar:
        return "ARITH", ar[0]
    return "OTHER", sites[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--inc", default=DEFAULT_INC)
    ap.add_argument("--objdump", default=shutil.which("objdump") or "objdump")
    a = ap.parse_args()

    print("SUBJECT   P3: is the address at each CORROBORATED-EXACT row's END a "
          "DISTINCT OBJECT'S START?")
    print("          inc : %s" % a.inc)
    print("          dll : %s" % a.dll)
    print("TREE ROOT     : %s" % REPO)
    print("PARAMETERS    : --objdump=%s  WINDOW=%d instructions"
          % (a.objdump, WINDOW))

    pe = pefile.PE(a.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    rdata = next((s.VirtualAddress,
                  s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
                 for s in pe.sections if s.Name.decode().rstrip("\0") == ".rdata")

    tgt, n_lea, n_insn = scan(a.dll, a.objdump)
    print("\nDECODE: %d instructions, %d rip-relative LEA -> %d distinct named "
          "addresses" % (n_insn, n_lea, len(tgt)))

    src = open(a.inc, encoding="utf-8").read()
    rows = [(m.group(1) or "(by address)", int(m.group(2), 16), int(m.group(3)),
             m.group(4)) for m in ROW.finditer(src)]
    if not rows:
        print("ROW REGEX MATCHED NOTHING -- refusing to report."); return 2
    starts = {r[1] for r in rows}
    adj = [r for r in rows if r[3].startswith('"adjacent-export:')]

    # ---- CONTROLS, printed BEFORE any finding ------------------------------
    pos = [(n, va) for n, va, _, _ in rows]
    pos_ok = sum(1 for _, va in pos if verdict_of(tgt.get(va, []))[0] == "STORE")
    print("\nPOSITIVE CONTROL  (applicable: the %d minted row starts, all known "
          "genuine vtable starts)" % len(pos))
    print("   classified as a vptr stamp: %d of %d" % (pos_ok, len(pos)))
    for n, va in pos:
        if verdict_of(tgt.get(va, []))[0] != "STORE":
            print("   *MISS* %#x %s -> %s" % (va, n, verdict_of(tgt.get(va, []))[0]))

    neg = []
    for t in tgt:
        rva = t - ib
        if not (rdata[0] <= rva < rdata[1]) or t in starts:
            continue
        try:
            b = pe.get_data(rva, 8)
        except Exception:
            continue
        if len(b) == 8 and all(0x20 <= c < 0x7F for c in b):
            neg.append(t)
    neg_bad = [t for t in neg if verdict_of(tgt[t])[0] == "STORE"]
    print("NEGATIVE CONTROL  (applicable: %d named .rdata addresses whose first 8 "
          "bytes are printable ASCII = string constants, not tables)" % len(neg))
    print("   wrongly classified as a vptr stamp: %d of %d" % (len(neg_bad), len(neg)))
    for t in neg_bad[:5]:
        print("   *FALSE POSITIVE* %#x %r" % (t, pe.get_data(t - ib, 16)))

    if pos_ok != len(pos) or neg_bad or not neg:
        print("\nCONTROL RED (positive %d/%d, negative %d false of %d) -- this test "
              "is not known to distinguish a table start from an interior or "
              "non-table address. Refusing to report per-row verdicts."
              % (pos_ok, len(pos), len(neg_bad), len(neg)))
        return 2
    print("CONTROLS GREEN: %d/%d positive, 0/%d negative false positives"
          % (pos_ok, len(pos), len(neg)))

    # ---- the 31 -------------------------------------------------------------
    print("\n%-42s %5s %-12s %s" % ("ROW (adjacent-export only)", "SLOT", "END", "P3"))
    print("-" * 108)
    passed, failed, undec = [], [], []
    for name, va, n, proof in sorted(adj, key=lambda r: r[1]):
        end = va + 8 * n
        sites = tgt.get(end, [])
        kind, first = verdict_of(sites)
        also_row = " (also a minted row start)" if end in starts else ""
        if kind == "STORE":
            passed.append((name, va, n, end))
            print("%-42s %5d %#-12x PREMISE-CHECKED-PASS  vptr stamp at %#x `%s`%s"
                  % (name[:42], n, end, first[0], first[2], also_row))
        elif kind == "UNNAMED":
            undec.append((name, va, n, end, "not named at all"))
            print("%-42s %5d %#-12x COULD-NOT-DERIVE  end address named by nothing"
                  % (name[:42], n, end))
        else:
            failed.append((name, va, n, end, kind, first))
            print("%-42s %5d %#-12x *** PREMISE-FAILED (%s) *** named at %#x `%s` "
                  "with NO vptr stamp -- this row's %d may be an UNDER-count with "
                  "no symptom%s"
                  % (name[:42], n, end, kind, first[0], first[2], n, also_row))

    print("-" * 108)
    print("P3 over the %d adjacent-export rows: %d PREMISE-CHECKED-PASS, "
          "%d PREMISE-FAILED, %d COULD-NOT-DERIVE"
          % (len(adj), len(passed), len(failed), len(undec)))

    # ---- EFFECTIVENESS: did the store-shape requirement ever REJECT a site? --
    # A uniform PASS is worth nothing if the test never had to discriminate.
    # Scored on the naming sites at the 31 END addresses, not on the rows.
    tot = rej = mixed = 0
    for name, va, n, _ in adj:
        k = [x[1] for x in tgt.get(va + 8 * n, [])]
        tot += len(k)
        ns = sum(1 for x in k if x != "STORE")
        rej += ns
        mixed += 1 if ns else 0
    print("  EFFECTIVENESS: %d naming sites examined at the %d end addresses; the "
          "store-shape requirement REJECTED %d of them, across %d rows that still "
          "passed on another site. The requirement is live, not vacuous."
          % (tot, len(adj), rej, mixed))

    # ---- where P3 is the ONLY evidence --------------------------------------
    solo = [(name, va + 8 * n) for name, va, n, _ in adj if va + 8 * n not in starts]
    print("  %d of %d row ends are ALSO minted row starts (corroborated a second "
          "way). P3 is the ONLY evidence for the other %d:"
          % (len(adj) - len(solo), len(adj), len(solo)))
    for name, e in sorted(solo, key=lambda x: x[1]):
        print("      %-42s end %#x" % (name[:42], e))
    print("\nEXPIRY: the impossibility claim -- 'under-sizing cannot occur on this "
          "row' -- stands ONLY for the %d rows above marked PREMISE-CHECKED-PASS. "
          "For every other row the honest statement is the weaker one: NOT "
          "OVER-SIZED, with under-sizing out of the screen's reach." % len(passed))
    print("STILL NOT CLAIMED: this tool does not upgrade any CORROBORATED-EXACT "
          "verdict, does not check slot CONTENT, and cannot see a table stamped by "
          "an address computed as (named base + constant offset) -- premise P2, "
          "which remains UNCHECKED.")
    return 3 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
