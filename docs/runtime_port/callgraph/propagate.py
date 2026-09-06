import csv, re, json
from collections import defaultdict

BASE = "docs/runtime_port/callgraph"

def load_edges(path):
    callees = defaultdict(set)
    callers = defaultdict(set)
    with open(path, encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for a, b in r:
            callees[a].add(b)
            callers[b].add(a)
    return callees, callers

profile_callees, profile_callers = load_edges(f"{BASE}/profile_edges.csv")
retail_callees, retail_callers = load_edges(f"{BASE}/retail_edges.csv")

FUN_RE = re.compile(r"^FUN_([0-9a-f]{8,9})$")
UNWIND_RE = re.compile(r"^Unwind@")

def key(name, side):
    # side: 'profile' or 'retail' -- used only for bookkeeping, key itself is address for FUN_, else name
    m = FUN_RE.match(name)
    if m:
        return ("FUN", m.group(1))
    if UNWIND_RE.match(name):
        return ("UNWIND", name)
    return ("NAMED", name)

def norm_addr(a):
    return a.lower().lstrip("0x")

# seed anchor map: profile addr (hex, no 0x) -> retail addr (hex, no 0x), from hash rows only
seed_rows = []
with open("docs/runtime_port/profile_to_retail.csv", encoding="utf-8") as f:
    r = csv.DictReader(f)
    for row in r:
        if row["method"] == "hash":
            seed_rows.append(row)

anchor_p2r = {}   # profile addr -> retail addr
anchor_r2p = {}   # retail addr -> profile addr
name_seed = {}    # profile FUN_addr -> retail_name  (proposed name if retail side still FUN_, else retail already named)

for row in seed_rows:
    pa = norm_addr(row["profile_addr"])
    ra = norm_addr(row["retail_addr"])
    anchor_p2r[pa] = ra
    anchor_r2p[ra] = pa

print("seeded anchors:", len(anchor_p2r))

# Build node-name lookup: profile addr -> current profile name (from graph node strings when FUN_, else real name)
# We also need: given a retail FUN_ addr, and a profile addr resolved to it, what "identity" transfers?
# We use the PROFILE name (could itself be FUN_ if profile function is also unnamed) or, if profile addr has a
# string-anchor file attribution (from profile_string_anchors.csv), carry that instead.

string_anchor = {}  # profile addr (no 0x) -> (dir,file,confidence)
with open("docs/runtime_port/profile_string_anchors.csv", encoding="utf-8") as f:
    r = csv.DictReader(f)
    for row in r:
        pa = norm_addr(row["profile_addr"])
        if row.get("inferred_file"):
            string_anchor[pa] = (row["inferred_dir"], row["inferred_file"], row["confidence"])

def resolve_key_via_anchor(k, from_side):
    """Given a key (kind, val) on `from_side` ('profile' or 'retail'), return the corresponding
    key on the other side if resolvable via current anchor map, else None."""
    kind, val = k
    if kind == "NAMED":
        return k  # assume names are shared/consistent across both programs once assigned
    if kind == "UNWIND":
        return ("IGNORE", val)
    if kind == "FUN":
        if from_side == "profile":
            r = anchor_p2r.get(val)
            if r:
                return ("FUN", r)
        else:
            p = anchor_r2p.get(val)
            if p:
                return ("FUN", p)
        return None
    return None

results = []  # dict rows for csv
round_no = 0
newly_added_this_round = True
total_rounds_used = 0

while newly_added_this_round:
    round_no += 1
    newly_added_this_round = False
    new_pairs_this_round = []

    # iterate over all anchor pairs (profile addr <-> retail addr) established so far
    for pa, ra in list(anchor_p2r.items()):
        pname = f"FUN_{pa}"
        rname = f"FUN_{ra}"
        for direction, pmap, rmap in (
            ("callee", profile_callees, retail_callees),
            ("caller", profile_callers, retail_callers),
        ):
            p_neighbors = pmap.get(pname, set())
            r_neighbors = rmap.get(rname, set())
            if not p_neighbors or not r_neighbors:
                continue

            p_unresolved = []
            r_unresolved = []
            p_named_set = set()
            r_named_set = set()
            skip_pair = False
            for n in p_neighbors:
                k = key(n, "profile")
                if k[0] == "UNWIND":
                    continue
                rk = resolve_key_via_anchor(k, "profile")
                if rk is None:
                    p_unresolved.append(k)
                elif rk[0] == "NAMED":
                    p_named_set.add(rk[1])
                # else: FUN resolved via anchor -> considered anchored, drop
            for n in r_neighbors:
                k = key(n, "retail")
                if k[0] == "UNWIND":
                    continue
                rk = resolve_key_via_anchor(k, "retail")
                if rk is None:
                    r_unresolved.append(k)
                elif rk[0] == "NAMED":
                    r_named_set.add(rk[1])

            if len(p_unresolved) == 1 and len(r_unresolved) == 1:
                pk = p_unresolved[0]
                rk = r_unresolved[0]
                if pk[0] != "FUN" or rk[0] != "FUN":
                    continue
                p2, r2 = pk[1], rk[1]
                if p2 in anchor_p2r or r2 in anchor_r2p:
                    continue  # already resolved elsewhere, skip to avoid conflicting evidence
                new_pairs_this_round.append((p2, r2, direction, pa, ra))

    # dedupe / check consistency: a p2 might be proposed by multiple anchor pairs; require agreement
    prop_p2r = defaultdict(set)
    evidence = defaultdict(list)
    for p2, r2, direction, pa, ra in new_pairs_this_round:
        prop_p2r[p2].add(r2)
        evidence[(p2, r2)].append((direction, pa, ra))

    accepted = 0
    conflicted = 0
    for p2, r2set in prop_p2r.items():
        if p2 in anchor_p2r:
            continue
        if len(r2set) != 1:
            conflicted += 1
            continue
        r2 = next(iter(r2set))
        if r2 in anchor_r2p:
            continue
        anchor_p2r[p2] = r2
        anchor_r2p[r2] = p2
        ev = evidence[(p2, r2)]
        results.append({
            "profile_addr": p2, "retail_addr": r2, "round": round_no,
            "direction": ";".join(sorted(set(d for d,_,_ in ev))),
            "arity": "1/1",
            "n_anchor_evidence": len(ev),
            "evidence": ";".join(f"{d}:{pa}->{ra}" for d,pa,ra in ev[:5]),
        })
        accepted += 1
        newly_added_this_round = True

    print(f"round {round_no}: proposed={len(new_pairs_this_round)} accepted={accepted} conflicted_multi_target={conflicted}")
    total_rounds_used = round_no
    if round_no > 40:
        print("safety stop at 40 rounds")
        break

print("TOTAL new pairs resolved:", len(results))
print("final anchor set size:", len(anchor_p2r), "(seed was", len(seed_rows), ")")

with open(f"{BASE}/callgraph_propagation_raw.json", "w", encoding="utf-8") as f:
    json.dump(results, f, indent=1)
