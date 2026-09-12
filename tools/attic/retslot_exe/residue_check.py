"""Independent cross-check of the extractor, by a DIFFERENT method.

Instead of a regex anchored on `fn_`, split engine_funcs.cpp into statements on
';' and bucket every statement that mentions ResolveAddr(Module::Exe by what its
left-hand side is. If the extractor is complete, the set of statements whose LHS
is an fn_* identifier must be exactly the extractor's 254, and every other
statement must have a non-fn_ LHS I can eyeball.
"""
import os, re, sys, csv
from collections import Counter

ROOT = sys.argv[1]
CSV = sys.argv[2]
SRC = os.path.join(ROOT, "engine", "funcs", "engine_funcs.cpp")
raw = open(SRC, encoding="utf-8", errors="replace").read()

# strip // comments and /* */ comments so prose cannot be counted as code
noblock = re.sub(r"/\*.*?\*/", "", raw, flags=re.S)
nocomment = re.sub(r"//[^\n]*", "", noblock)

stmts = nocomment.split(";")
hits = [s for s in stmts if re.search(r"ResolveAddr\s*\(\s*Module::Exe", s)]
print("statements mentioning ResolveAddr(Module::Exe (comments stripped): %d" % len(hits))

fnlhs, otherlhs = [], []
for s in hits:
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*=[^=]", s)
    lhs = m.group(1) if m else "<no-assignment>"
    (fnlhs if lhs.startswith("fn_") else otherlhs).append((lhs, " ".join(s.split())[:100]))

print("  LHS is fn_*      : %d" % len(fnlhs))
print("  LHS is something else / no assignment: %d" % len(otherlhs))
print("  other LHS kinds  : %s" % Counter(l for l, _ in otherlhs).most_common(12))

extracted = {r["fn"] for r in csv.DictReader(open(CSV, encoding="utf-8"))}
found = {l for l, _ in fnlhs}
print("\nextractor fn set size %d ; statement-split fn set size %d" % (len(extracted), len(found)))
print("in statement-split but NOT extracted: %s" % sorted(found - extracted))
print("in extracted but NOT statement-split: %s" % sorted(extracted - found))

# comment-only mentions, for the record
print("\nResolveAddr(Module::Exe mentions inside COMMENTS: %d"
      % (len(re.findall(r"ResolveAddr\s*\(\s*Module::Exe", raw))
         - len(re.findall(r"ResolveAddr\s*\(\s*Module::Exe", nocomment))))
