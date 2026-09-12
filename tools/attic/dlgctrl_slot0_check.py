#!/usr/bin/env python3
"""Lane FINAL re-derivation: for every retail UI::DialogController subclass
vtable, is slot 0 (the deleting destructor) INHERITED from the base?

An inherited slot in the exe is a 6-byte `FF 25 rel32` thunk whose target is a
PE import slot the import table names.  An overridden slot is an ordinary exe
body.  This asks the question directly, over the FULL universe of 39 (lane
VTFID's comparison table drops SelectResourceUI, which has no standalone table
of ours -- it is still a retail subclass and still counts for the claim).
"""
import sys, os
sys.path.insert(0, os.path.abspath("tools"))
import dlgctrl_vtable_audit as A

rows  = A.csv_rows(".")
uni   = A.universe(rows)
stubs = A.typename_stubs({r["storage"]: r["name"] for r in rows if r["storage"]})
found = A.locate(stubs)

print("universe:", len(uni), " located:", sum(1 for n in uni if found.get(n)))
inherited0, own0, amb = [], [], []
for name in sorted(uni):
    cand = found.get(name)
    if not cand:
        amb.append(name); continue
    if len(cand) != 1:
        amb.append(name + "(multi)"); continue
    base, slots = cand[0]
    nm = A.thunk_name(slots[0])
    (inherited0 if nm else own0).append((name, hex(base), hex(slots[0]), nm))

print()
print("slot 0 INHERITED (a named import thunk):", len(inherited0))
for r in inherited0:
    print("   ", r)
print("slot 0 OVERRIDDEN (own exe body):", len(own0))
for n, b, s, _ in own0:
    print(f"    {n:<22} vt={b}  slot0={s}")
print("unresolved:", amb)

# The same question for slots 1 and 2, which lane VTFID also claims nobody
# inherits -- checked here rather than inherited from its prose.
for si in (1, 2):
    inh = 0
    for name in sorted(uni):
        cand = found.get(name)
        if not cand or len(cand) != 1:
            continue
        if A.thunk_name(cand[0][1][si]):
            inh += 1
    print(f"slot {si} inherited by: {inh} of {len(uni)}")
