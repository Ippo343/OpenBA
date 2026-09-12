"""Second, INDEPENDENT arity screen: retail's own CALL SITES vs our typedef.

Why this exists. The first screen compared Ghidra's declared parameter count to
the typedef, and Ghidra's count is SignatureSource.ANALYSIS for 231 of 234 -- it
proved to over-count (four rows) AND under-count (seven Dialog ctors, nparams=1
where the call sites set four registers plus a stack argument). An under-count is
the dangerous direction: it would bucket a genuinely SHORT typedef as
"Ghidra under-counted, ignore".

This screen does not ask Ghidra what the signature is. It asks what retail's own
compiler EMITTED: the highest MS-x64 argument register written on the run-up to
each call, and whether a fifth-and-beyond stack argument was stored. A hidden
return slot is an extra LEADING pointer argument, so a typedef missing one is
short by at least one register -- which this measures directly.

Flag rule: observed argument count > typedef argument count.
"""
import csv, sys
from collections import Counter

corpus, arity = sys.argv[1], sys.argv[2]

g = {}
for r in csv.DictReader(open(arity, encoding="utf-8"), delimiter="\t"):
    g[r["addr"].lower()] = r

flag, clean, nosite = [], [], []
for r in csv.DictReader(open(corpus, encoding="utf-8")):
    if r["bucket"] != "exe-text":
        continue
    d = g[r["addr"]]
    sites = int(d["sites"])
    obs = int(d["maxreg"])
    stack = d["stackarg"] == "true"
    tn = int(r["nargs"])
    if sites == 0:
        nosite.append((r, d))
    # The `stack` column is DELIBERATELY NOT part of the flag rule. It was, and
    # it produced 71 false positives: `MOV [RSP+0x20..0x38], reg` is the ordinary
    # MSVC shadow-space / callee-saved spill at almost every call site, not a
    # fifth argument, so the rule fired on rows whose register count already
    # matched. Dropping it costs nothing HERE because a hidden return slot is
    # argument 1 or 2 and therefore always lands in a REGISTER -- it can never
    # hide in the stack region this column was trying to see.
    elif obs > tn:
        flag.append((r, d, obs, tn, stack))
    else:
        clean.append((r, d))

print("exe-text rows: %d" % (len(flag) + len(clean) + len(nosite)))
print("  no call site found in the exe (cannot measure): %d" % len(nosite))
print("  observed args <= typedef args (CLEAN)         : %d" % len(clean))
print("  observed args >  typedef args (FLAG)          : %d" % len(flag))

print("\n=== FLAGGED ===")
for r, d, obs, tn, stack in sorted(flag, key=lambda x: -(x[2] - x[3])):
    print("  %-46s %s  sites=%-3s obs_regs=%d typedef_args=%d%s"
          % (r["fn"], r["addr"], d["sites"], obs, tn, "  +stackarg" if stack else ""))
    print("      ghidra name %s ; regs written %s" % (d["name"], d["regs"]))
    print("      typedef %s (%s)" % (r["ret"], r["args"][:100]))

print("\n=== NO CALL SITE (unmeasurable by this screen) ===")
for r, d in nosite:
    print("  %-46s %s  %s  typedef args=%s" % (r["fn"], r["addr"], d["name"], r["nargs"]))
