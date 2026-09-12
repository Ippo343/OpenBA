"""Join the source-side corpus (typedef) to the Ghidra-side dump (real function)
and bucket each row by how much evidence there is that the typedef is wrong.

NOTE ON WHY ARITY IS THE PRIMARY SIGNAL HERE. 231 of the 234 Ghidra signatures
are SignatureSource.ANALYSIS -- auto-derived, and the project has already been
burned by trusting Ghidra's return types (see reference_ghidra_c_output_vs_
disassembly). Ghidra declared ZERO struct-by-value returns across the whole
corpus, so "no STRUCT return => clean" would be a sweep that says nothing.

A hidden return slot is an EXTRA LEADING POINTER PARAMETER. So it cannot be
invisible to arity: if the real function takes a slot and the typedef does not,
the typedef is short by one. Arity disagreement is therefore the screen; the
disassembly decides each survivor.
"""
import csv, sys
from collections import Counter

corpus = sys.argv[1]
dump = sys.argv[2]

g = {}
for r in csv.DictReader(open(dump, encoding="utf-8"), delimiter="\t"):
    g[r["addr"].lower()] = r

rows = []
for r in csv.DictReader(open(corpus, encoding="utf-8")):
    if r["bucket"] != "exe-text":
        continue
    d = g.get(r["addr"])
    r["g"] = d
    rows.append(r)

print("joined rows: %d ; unmatched in dump: %d"
      % (len(rows), sum(1 for r in rows if r["g"] is None)))

# suspects: Ghidra return is a POINTER TO A NAMED (non-undefined) TYPE.
# That is what a by-value struct return looks like to an analyser that has not
# been told about the hidden slot: it sees the callee hand back the pointer it
# was given and calls the return a T*.
susp, arity, clean = [], [], []
for r in rows:
    d = r["g"]
    if d is None:
        continue
    gret = d["retname"]
    gn = int(d["nparams"]) if d["nparams"].isdigit() else -1
    tn = int(r["nargs"]) if r["nargs"].isdigit() else -1
    named_ptr = gret.endswith("*") and not gret.startswith("undefined") and gret != "void *"
    if gn != tn:
        arity.append((r, d, gn, tn))
    if named_ptr:
        susp.append((r, d))

print("\n=== Ghidra return is a pointer to a NAMED type (by-value-return look-alike): %d ===" % len(susp))
for r, d in susp:
    print("  %-46s %s  ret=%-18s gparams=%s  typedef(%s) ret=%s args=%s"
          % (r["fn"], r["addr"], d["retname"], d["nparams"], r["nargs"], r["ret"], r["args"][:70]))

print("\n=== ARITY DISAGREEMENT (Ghidra nparams != typedef nargs): %d ===" % len(arity))
for r, d, gn, tn in sorted(arity, key=lambda x: x[2] - x[3]):
    print("  %-46s %s  ghidra=%d typedef=%d  delta=%+d  gret=%-16s %s"
          % (r["fn"], r["addr"], gn, tn, gn - tn, d["retname"], d["name"][:40]))

print("\ndelta histogram: %s" % Counter(gn - tn for _, _, gn, tn in arity))
