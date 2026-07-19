"""Type-id slot cross-check ("the xref check", 2026-07-19).

Joins every 0x1401cXXXX literal in repo code against the Ghidra-derived truth in
standalone/factory_registrations.csv (each row's RegisterType writer constructs
Object::BaseTypeInfo(&slot, "TypeName", ...) — the slot's identity is definitive).
Flags lines whose Component/Description-ish identifiers contradict the truth name.

Found on first run: 3 fill-value bugs (c7350 "StateMachineComponent" vs
ConsumeProduceComponent; buildcomponent fallbacks c7c10/c7ac0) plus ~15 misnomer
symbols. Run after any wave that adds type-id resolves/fills. Output classes:
SUSPICIOUS = review by hand (heuristic; many are just prose), NOT IN CSV =
non-factory slots (game-state ptrs, goal/tutorial TypeInfo storages) — verify via
Ghidra get_xrefs_to [DATA] writer if a type-id is claimed, CLEAN = name matches.
"""
import csv, re, os, collections

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV = os.path.join(REPO, "standalone", "factory_registrations.csv")

# 1) truth: slot addr -> type name, from the Ghidra-generated RegisterType sweep
truth = {}
with open(CSV, newline="", encoding="utf-8", errors="replace") as f:
    for row in csv.reader(f):
        if len(row) < 4:
            continue
        m = re.search(r"1401c[0-9a-f]{3,4}", row[2], re.I)
        if not m:
            continue
        name = row[3].replace('"', "").strip()
        truth[m.group(0).lower()] = name

# 2) scan repo code for 0x1401cXXXX mentions
pat = re.compile(r"0x(1401c[0-9a-f]{4})", re.I)
uses = collections.defaultdict(list)  # addr -> [(file,line,text)]
for root, dirs, files in os.walk(REPO):
    rel = os.path.relpath(root, REPO)
    if rel.split(os.sep)[0] in (".git", "notes", "decomp", "tools", ".vscode"):
        dirs[:] = []
        continue
    for fn in files:
        if not fn.endswith((".cpp", ".h")):
            continue
        p = os.path.join(root, fn)
        try:
            lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
        except OSError:
            continue
        for i, ln in enumerate(lines, 1):
            for m in pat.finditer(ln):
                uses[m.group(1).lower()].append((os.path.relpath(p, REPO), i, ln.strip()[:160]))

# 3) report: per address, truth name + whether any use-line mentions a conflicting name
word = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
suspicious, clean, unknown = [], [], []
for addr in sorted(uses):
    t = truth.get(addr)
    entries = uses[addr]
    if t is None:
        unknown.append((addr, entries))
        continue
    flagged = []
    for (f, i, ln) in entries:
        # a line is suspicious if it names a Component/Description-ish identifier
        # that is NOT the truth name (substring-insensitive)
        ids = [w for w in word.findall(ln) if ("Component" in w or "Description" in w or "Entity" in w) and len(w) > 9]
        bad = [w for w in ids if t.lower() not in w.lower() and w.lower() not in t.lower()]
        if bad:
            flagged.append((f, i, ln, sorted(set(bad))))
    if flagged:
        suspicious.append((addr, t, flagged))
    else:
        clean.append((addr, t, len(entries)))

print("=== SUSPICIOUS (line names a type != CSV truth) ===")
for addr, t, flagged in suspicious:
    print(f"\n0x{addr}  TRUTH={t}")
    for f, i, ln, bad in flagged:
        print(f"  {f}:{i}  [{','.join(bad)}]")
        print(f"      {ln}")

print("\n=== NOT IN CSV (need live Ghidra xref) ===")
for addr, entries in unknown:
    print(f"0x{addr}  ({len(entries)} uses)  e.g. {entries[0][0]}:{entries[0][1]}  {entries[0][2][:100]}")

print(f"\n=== CLEAN ===")
for addr, t, n in clean:
    print(f"0x{addr}  {t}  ({n} uses)")
print(f"\ntotals: suspicious={len(suspicious)} unknown={len(unknown)} clean={len(clean)}")
