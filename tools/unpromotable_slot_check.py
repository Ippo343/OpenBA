#!/usr/bin/env python3
"""Check every RT_UNPROMOTABLE tag's slot count against the minted table it names.

WHY
---
The `RT_UNPROMOTABLE:` block in ``standalone/reg_mint_promoted.cpp`` is the
source of truth for ``tools/runtime_census.py``'s per-namespace ceiling.  Its
slot-count field is PROSE, and prose describing a measurement decays silently
because nothing regenerates a comment.  One tag had carried
``??_7ReaderWriter@IO@@6B@ | 2/36`` since Wave 17; the row is 26 slots (Wave 19
correction C30, re-derived by Wave 20 lane EXTENT).  Nothing could have noticed:
the field is free text and the census derives no number from it.

WHAT IT COMPARES
----------------
The tag's denominator against the slot count of the SAME symbol's row in
``standalone/rt_data_mint_tables.inc`` -- a generated artefact derived from the
DLL, and the thing the mint actually stamps.

THE TWO CONVENTIONS, WHICH THE TAGS NEVER DECLARED
--------------------------------------------------
``N/M (K purecall)``  M = NON-purecall slots, so the table is M + K
                      (Object and Video rows; ``??_7Base@Object@@6B@ 2/2 (1
                      purecall)`` against a 3-slot row).
``N/M ..., K purecall``  M = the WHOLE table (the two IO rows;
                      ``??_7Stream@IO@@6B@ 1/7 ..., 5 purecall`` on 7 slots).

Both are self-consistent and they disagree by K.  This tool accepts EITHER and
reports a row only when NEITHER fits, so it cannot manufacture work out of a
convention it happens to dislike -- at the cost of being blind to a row that is
wrong by exactly K in the direction of the other reading.  That blindness is
stated here rather than discovered later.

CONTROLS (both must pass or it reports nothing and exits 2)
-----------------------------------------------------------
positive  a non-zero number of tags must parse AND a non-zero number of those
          must find their symbol in the .inc -- a tag naming a symbol the mint
          does not have is unscoreable, not clean.
negative  a synthetic symbol is injected into the tag set with a deliberately
          impossible count; it MUST be reported.  If it is not, the comparison
          is not comparing.

Usage:  python tools/unpromotable_slot_check.py [--root <repo root>]
Exit:   0 = every scoreable tag fits one of the two conventions
        1 = at least one does not (listed)
        2 = a control failed, nothing reported
"""

import argparse
import os
import re
import sys

TAGS = os.path.join("standalone", "reg_mint_promoted.cpp")
INC = os.path.join("standalone", "rt_data_mint_tables.inc")

TAG_RE = re.compile(r"RT_UNPROMOTABLE:\s*(\?\S+)\s*\|\s*([^|]+?)\s*\|")
NUM_RE = re.compile(r"^\s*([\d?]+)\s*/\s*(\d+)")
PURE_RE = re.compile(r"(\d+)\s*purecall")
ROW_RE = re.compile(r'\{\s*"(\?\?_7[^"]+)"\s*,\s*0x[0-9a-fA-F]+ull\s*,\s*(\d+)\s*,')

SYNTHETIC = ("??_7SyntheticControlRow@LaneDoors@@6B@", "0/999 (0 purecall)", 4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    print("SUBJECT ROOT : %s" % root)
    print("TAGS         : %s" % TAGS)
    print("MINT ROWS    : %s" % INC)
    print("SCOPE        : the DENOMINATOR only.  The numerator (slots ported) is "
          "not derivable from the .inc and is NOT checked.")

    tag_path, inc_path = os.path.join(root, TAGS), os.path.join(root, INC)
    for p in (tag_path, inc_path):
        if not os.path.isfile(p):
            print("CONTROL RED  : missing %s" % p)
            return 2

    rows = {}
    for m in ROW_RE.finditer(open(inc_path, encoding="utf-8", errors="replace").read()):
        rows[m.group(1)] = int(m.group(2))

    tags = []
    for i, line in enumerate(
            open(tag_path, encoding="utf-8", errors="replace").read().splitlines(), 1):
        m = TAG_RE.search(line)
        if m:
            tags.append((i, m.group(1), m.group(2)))

    # negative control: a row that must be reported
    rows[SYNTHETIC[0]] = SYNTHETIC[2]
    tags.append((0, SYNTHETIC[0], SYNTHETIC[1]))

    scoreable, bad, caught_synth = 0, [], False
    for lineno, sym, field in tags:
        num = NUM_RE.match(field)
        if not num:
            continue                      # "n/a (scalar ptr)" and friends
        mint = rows.get(sym)
        if mint is None:
            continue                      # not a minted vtable row
        scoreable += 1
        denom = int(num.group(2))
        pure = PURE_RE.search(field)
        k = int(pure.group(1)) if pure else 0
        if denom == mint or denom + k == mint:
            continue
        if sym == SYNTHETIC[0]:
            caught_synth = True
            continue
        bad.append((lineno, sym, field, mint, k))

    pos_ok = scoreable > 1                # > 1: the synthetic is one of them
    print("CONTROL pos  : %d tags parsed, %d of them scoreable against a minted "
          "row -> %s" % (len(tags) - 1, scoreable - 1, "GREEN" if pos_ok else "RED"))
    print("CONTROL neg  : synthetic 0/999 row reported -> %s"
          % ("GREEN" if caught_synth else "RED"))
    if not pos_ok or not caught_synth:
        print("*** MEASURED NOTHING: a control failed.  No verdict. ***")
        return 2

    for lineno, sym, field, mint, k in bad:
        print("STALE  %s:%d  %s\n         tag says %r; the mint row is %d slots "
              "(neither M=%d nor M+%d fits)" % (TAGS, lineno, sym, field, mint,
                                                mint, k))
    print("==== %d of %d scoreable tags fit one of the two conventions, %d STALE ===="
          % (scoreable - 1 - len(bad), scoreable - 1, len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
